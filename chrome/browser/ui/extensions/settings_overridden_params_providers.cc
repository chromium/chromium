// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/settings_overridden_params_providers.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/barrier_closure.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/single_thread_task_runner.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/extensions/controlled_home_dialog_controller.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_icon_placeholder.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/ui_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_set.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace settings_overridden_params {

namespace {

// Returns true if the given |template_url| corresponds to Google search.
bool IsGoogleSearch(const TemplateURL& template_url,
                    const TemplateURLService& template_url_service) {
  GURL search_url =
      template_url.GenerateSearchURL(template_url_service.search_terms_data());
  return google_util::IsGoogleSearchUrl(search_url);
}

// Returns true if Google is the default search provider.
bool GoogleIsDefaultSearchProvider(Profile* profile) {
  const TemplateURLService* const template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  const TemplateURL* const default_search =
      template_url_service->GetDefaultSearchProvider();
  if (!default_search) {
    // According to TemplateURLService, |default_search| can be null if the
    // default search engine is disabled by policy.
    return false;
  }

  return IsGoogleSearch(*default_search, *template_url_service);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// Returns the number of extensions that are currently enabled that override the
// default search setting.
size_t GetNumberOfExtensionsThatOverrideSearch(Profile* profile) {
  const auto* const registry = extensions::ExtensionRegistry::Get(profile);
  const auto overrides_search = [](auto extension) {
    auto* const settings = extensions::SettingsOverrides::Get(extension.get());
    return settings && settings->search_engine;
  };
  return std::ranges::count_if(registry->enabled_extensions(),
                               overrides_search);
}

struct SecondarySearchInfo {
  enum class Type {
    // Google is the secondary search engine.
    kGoogle,
    // The secondary search is one of the default-populated searches, but is
    // not Google.
    kNonGoogleInDefaultList,
    // Some other search engine is the secondary search.
    kOther,
  };

  Type type;

  // The origin of the search engine. Only populated if the secondary search
  // is not from another extension.
  GURL origin;

  // The name of the search engine, if available.
  std::u16string name;

  // The favicon URL, if available.
  GURL favicon_url;
};

// Returns details about the search that would take over, if the currently-
// controlling extension were to be disabled.
SecondarySearchInfo GetSecondarySearchInfo(Profile* profile) {
  // First, check if there's another extension that would take over.
  size_t num_overriding_extensions =
      GetNumberOfExtensionsThatOverrideSearch(profile);
  // This method should only be called when there's an extension that overrides
  // the search engine.
  DCHECK_GE(num_overriding_extensions, 1u);

  // Another extension would take over.
  std::optional<const TemplateURL> secondary_extension_search;
  if (num_overriding_extensions > 1) {
    const std::string& search_pref_key =
        DefaultSearchManager::kDefaultSearchProviderDataPrefName;

    ExtensionPrefValueMap* extension_prefs_value_map =
        ExtensionPrefValueMapFactory::GetForBrowserContext(profile);

    std::string primary_ext_id =
        extension_prefs_value_map->GetExtensionControllingPref(search_pref_key);

    const base::Value* secondary_pref =
        extension_prefs_value_map->GetEffectivePrefValue(
            search_pref_key, profile->IsIncognitoProfile(),
            /* from_incognito= */ nullptr,
            /* ignore_extension_id= */ primary_ext_id);

    if (secondary_pref) {
      std::unique_ptr<TemplateURLData> url_data =
          TemplateURLDataFromDictionary(secondary_pref->GetDict());
      if (url_data) {
        secondary_extension_search.emplace(std::move(*url_data));
      }
    }
  }

  const TemplateURLService* const template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  const TemplateURL* const secondary_search =
      secondary_extension_search
          ? &(*secondary_extension_search)
          : template_url_service->GetDefaultSearchProviderIgnoringExtensions();

  if (!secondary_search) {
    // We couldn't find a default (this could potentially happen if e.g. the
    // default search engine is disabled by policy).
    // TODO(devlin): It *seems* like in that case, extensions also shouldn't be
    // able to override it. Investigate.
    return {SecondarySearchInfo::Type::kOther};
  }

  const GURL search_url = secondary_search->GenerateSearchURL(
      template_url_service->search_terms_data());
  const GURL origin = search_url.DeprecatedGetOriginAsURL();
  SecondarySearchInfo search_info;
  search_info.origin = origin;
  search_info.name = secondary_search->short_name();
  if (base::FeatureList::IsEnabled(
          extensions_features::kSearchEngineExplicitChoiceDialog)) {
    search_info.favicon_url = secondary_search->favicon_url();
  }
  if (google_util::IsGoogleSearchUrl(search_url)) {
    search_info.type = SecondarySearchInfo::Type::kGoogle;
  } else if (template_url_service->ShowInDefaultList(secondary_search)) {
    search_info.type = SecondarySearchInfo::Type::kNonGoogleInDefaultList;
  } else {
    search_info.type = SecondarySearchInfo::Type::kOther;
  }

  return search_info;
}

inline constexpr extension_misc::ExtensionIcons kDialogIconSize =
    extension_misc::EXTENSION_ICON_SMALLISH;

// Creates a fallback icon for a search engine, in case a favicon can't be
// fetched. If an extension is provided, it derives an icon from that name.
ui::ImageModel CreateFallbackSearchIcon(const std::string& extension_name) {
  if (!extension_name.empty()) {
    return ui::ImageModel::FromImage(
        extensions::ExtensionIconPlaceholder::CreateImage(kDialogIconSize,
                                                          extension_name));
  }

  // This icon doesn't pertain to an extension. Fall back to a fixed icon.
  return ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon,
                                        ui::kColorIcon, kDialogIconSize);
}

// A descriptor containing information required to fetch an icon, and a pointer
// to the ImageModel that should be populated with the result of the fetch.
struct IconFetchParams {
  // The URL from which to fetch.
  GURL url;

  // If not empty, the name of the Extension to which this icon pertains. Used
  // to generate a fallback icon if necessary.
  std::string extension_name;

  // The destination image into which the fetched icon will be written.
  raw_ptr<ui::ImageModel> image = nullptr;
};

constexpr char kImageFetcherUmaClient[] = "Extensions";
inline constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation(
        "settings_overridden_dialog_icon_fetcher",
        R"(
            semantics {
              sender: "Image fetcher for the settings-overridden dialog"
              description:
                "Fetches a favicon for a search engine to display in the "
                "settings overridden dialog."
              trigger:
                "The user sees a dialog indicating their search settings have "
                "been overridden by an extension."
              data: "The URL of the search engine's favicon."
              destination: WEBSITE
              internal {
                contacts {
                  owners: "extensions/UI_OWNERS"
                }
              }
              user_data {
                type: NONE
              }
              last_reviewed: "2026-02-10"
            }
            policy {
              cookies_allowed: NO
              setting: "This feature cannot be disabled by settings."
              policy_exception_justification: "Not implemented."
            })");

// A utility function that kicks off asynchronous resource load operations, to
// collect one or more icon images. A supplied callback is invoked when all
// requested icons have been loaded (or a placeholder supplied, if unavailable).
void FetchIconsThenRun(std::vector<IconFetchParams>& lookups,
                       Profile* profile,
                       base::OnceClosure done_callback) {
  CHECK(base::FeatureList::IsEnabled(
      extensions_features::kSearchEngineExplicitChoiceDialog));

  // A barrier closure will invoke the callback when called once per resource.
  // Post the callback task so it always runs asynchronously, even if all
  // fetches are synchronous. This ensures that the callback doesn't run before
  // the code that's queuing it finishes (dangling pointer issues).
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      lookups.size(),
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         std::move(done_callback)));

  // If there were no lookups, the barrier closer will have just executed,
  // and we can bail out.
  if (lookups.empty()) {
    return;
  }

  image_fetcher::ImageFetcher* image_fetcher =
      ImageFetcherServiceFactory::GetForKey(profile->GetProfileKey())
          ->GetImageFetcher(image_fetcher::ImageFetcherConfig::kNetworkOnly);

  for (auto& lookup : lookups) {
    if (!lookup.url.is_valid() || !image_fetcher) {
      // Fallback if the service is unavailable.
      *lookup.image = CreateFallbackSearchIcon(lookup.extension_name);
      barrier_closure.Run();
      continue;
    }

    image_fetcher::ImageFetcherParams fetch_params(traffic_annotation,
                                                   kImageFetcherUmaClient);
    // Set desired frame size in case the URL is for a multi-resolution .ico
    // file. After the fetch, a resize is done to the dialog's required icon
    // size.
    fetch_params.set_frame_size(gfx::Size(kDialogIconSize, kDialogIconSize));

    image_fetcher->FetchImage(
        lookup.url,
        base::BindOnce(
            [](const std::string& extension_name, ui::ImageModel* image_spot,
               base::RepeatingClosure barrier, const gfx::Image& image,
               const image_fetcher::RequestMetadata& metadata) {
              if (!image.IsEmpty()) {
                // Resize the result to the intended icon size.
                gfx::ImageSkia skia = image.AsImageSkia();
                gfx::Size icon_size(kDialogIconSize, kDialogIconSize);
                gfx::ImageSkia resized =
                    gfx::ImageSkiaOperations::CreateResizedImage(
                        skia, skia::ImageOperations::RESIZE_BEST, icon_size);

                *image_spot = ui::ImageModel::FromImageSkia(resized);
              } else {
                *image_spot = CreateFallbackSearchIcon(extension_name);
              }
              barrier.Run();
            },
            lookup.extension_name, lookup.image, barrier_closure),
        fetch_params);
  }
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

}  // namespace

std::optional<ExtensionSettingsOverriddenDialog::Params> GetNtpOverriddenParams(
    Profile* profile) {
  const GURL ntp_url(chrome::kChromeUINewTabURL);
  const extensions::Extension* extension =
      ExtensionWebUI::GetExtensionControllingURL(ntp_url, profile);
  if (!extension) {
    return std::nullopt;
  }

  // This preference tracks whether users have acknowledged the extension's
  // control, so that they are not warned twice about the same extension.
  const char* preference_name = extensions::kNtpOverridingExtensionAcknowledged;

  std::vector<GURL> possible_rewrites =
      content::BrowserURLHandler::GetInstance()->GetPossibleRewrites(ntp_url,
                                                                     profile);
  // We already know that the extension is the primary NTP controller.
  DCHECK(!possible_rewrites.empty());
  DCHECK_EQ(extension->url().host(), possible_rewrites[0].host())
      << "Unexpected NTP URL: " << possible_rewrites[0];

  // Find whether the default NTP would take over if the extension were to be
  // removed. This might not be the case if, e.g. an enterprise policy set the
  // NTP or the default search provided its own.
  bool default_ntp_is_secondary = true;
  if (possible_rewrites.size() > 1) {
    default_ntp_is_secondary =
        possible_rewrites[1] == ntp_url ||
        possible_rewrites[1] == GURL(chrome::kChromeUINewTabPageURL) ||
        possible_rewrites[1] == GURL(chrome::kChromeUINewTabPageThirdPartyURL);
  }
  // Check if there's another extension that would take over (this isn't
  // included in BrowserURLHandler::GetPossibleRewrites(), which only takes the
  // highest-priority from each source).
  default_ntp_is_secondary &=
      ExtensionWebUI::GetNumberOfExtensionsOverridingURL(ntp_url, profile) == 1;

  // We show different dialogs based on whether the NTP would return to the
  // default Chrome NTP with Google search.
  bool use_back_to_google_messaging =
      default_ntp_is_secondary && GoogleIsDefaultSearchProvider(profile);

  constexpr char kGenericDialogHistogramName[] =
      "Extensions.SettingsOverridden.GenericNtpOverriddenDialogResult";
  constexpr char kBackToGoogleDialogHistogramName[] =
      "Extensions.SettingsOverridden.BackToGoogleNtpOverriddenDialogResult";

  std::u16string dialog_title;
  const char* histogram_name = nullptr;
  const gfx::VectorIcon* icon = nullptr;
  if (use_back_to_google_messaging) {
    dialog_title = l10n_util::GetStringUTF16(
        IDS_EXTENSION_NTP_OVERRIDDEN_DIALOG_TITLE_BACK_TO_GOOGLE);
    histogram_name = kBackToGoogleDialogHistogramName;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    icon = &vector_icons::kGoogleGLogoIcon;
#endif
  } else {
    dialog_title = l10n_util::GetStringUTF16(
        IDS_EXTENSION_NTP_OVERRIDDEN_DIALOG_TITLE_GENERIC);
    histogram_name = kGenericDialogHistogramName;
  }
  DCHECK(!dialog_title.empty());
  DCHECK(histogram_name);

  std::u16string dialog_message = l10n_util::GetStringFUTF16(
      IDS_EXTENSION_NTP_OVERRIDDEN_DIALOG_BODY_GENERIC,
      extensions::ui_util::GetFixupExtensionNameForUIDisplay(
          extension->name()));

  SettingsOverriddenDialogController::ShowParams show_params(
      std::move(dialog_title), std::move(dialog_message), icon);
  return ExtensionSettingsOverriddenDialog::Params(
      extension->id(), preference_name, histogram_name, std::move(show_params));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
void GetSearchOverriddenParamsThenRun(
    content::WebContents* web_contents,
    base::OnceCallback<
        void(std::unique_ptr<ExtensionSettingsOverriddenDialog::Params>)>
        done_callback) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile) {
    std::move(done_callback).Run(nullptr);
    return;
  }

  const extensions::Extension* extension =
      extensions::GetExtensionOverridingSearchEngine(profile);
  if (!extension) {
    std::move(done_callback).Run(nullptr);
    return;
  }

  // For historical reasons, the search override preference is the same as the
  // one we use for the controlled home setting. We continue this so that
  // users won't see the bubble or dialog UI if they've already acknowledged
  // an older version.
  const char* preference_name =
      ControlledHomeDialogController::kAcknowledgedPreference;

  // Find the active search engine (which is provided by the extension).
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(template_url_service->IsExtensionControlledDefaultSearch());
  const TemplateURL* default_search =
      template_url_service->GetDefaultSearchProvider();
  DCHECK(default_search);
  DCHECK_EQ(TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION,
            default_search->type());

  // NOTE: For most TemplateURLs, there's no guarantee that search_url is a
  // valid URL (it could contain placeholders, etc). However, for extension-
  // provided search engines, we require they be valid URLs.
  GURL search_url(default_search->url());
  DCHECK(search_url.is_valid()) << default_search->url();

  // Check whether the secondary search is the same search the extension set.
  // This can happen if the user set a search engine, and then installed an
  // extension that set the same one.
  SecondarySearchInfo secondary_search = GetSecondarySearchInfo(profile);
  // NOTE: Normally, we wouldn't want to use direct equality comparison of
  // GURL::GetOrigin() because of edge cases like inner URLs with filesystem,
  // etc. This okay here, because if the origins don't match, we'll show the
  // dialog to the user. That's likely good if any extension is doing something
  // as crazy as using filesystem: URLs as a search engine.
  if (!secondary_search.origin.is_empty() &&
      secondary_search.origin == search_url.DeprecatedGetOriginAsURL()) {
    std::move(done_callback).Run(nullptr);
    return;
  }

  // Format the URL for display.
  std::u16string formatted_search_url =
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          search_url);

  constexpr char kGenericDialogHistogramName[] =
      "Extensions.SettingsOverridden.GenericSearchOverriddenDialogResult";
  constexpr char kBackToOtherHistogramName[] =
      "Extensions.SettingsOverridden.BackToOtherSearchOverriddenDialogResult";
  constexpr char kBackToGoogleHistogramName[] =
      "Extensions.SettingsOverridden.BackToGoogleSearchOverriddenDialogResult";

  const char* histogram_name = nullptr;
  switch (secondary_search.type) {
    case SecondarySearchInfo::Type::kGoogle:
      histogram_name = kBackToGoogleHistogramName;
      break;
    case SecondarySearchInfo::Type::kNonGoogleInDefaultList:
      DCHECK(!secondary_search.name.empty());
      histogram_name = kBackToOtherHistogramName;
      break;
    case SecondarySearchInfo::Type::kOther:
      histogram_name = kGenericDialogHistogramName;
      break;
  }

  // The parameters fetched will depend on the style of dialog being shown.
  if (base::FeatureList::IsEnabled(
          extensions_features::kSearchEngineExplicitChoiceDialog)) {
    // Build an explicit-choice dialog.
    std::u16string dialog_title = l10n_util::GetStringUTF16(
        IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_TITLE_EXPLICIT_CHOICE);
    const std::u16string extension_name_for_ui =
        extensions::ui_util::GetFixupExtensionNameForUIDisplay(
            extension->name());
    std::u16string dialog_message = l10n_util::GetStringFUTF16(
        IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_BODY_EXPLICIT_CHOICE,
        extension_name_for_ui, default_search->short_name());

    // On the 'explicit choice' dialog, there is no icon at the title level.
    SettingsOverriddenDialogController::ShowParams show_params(
        std::move(dialog_title), std::move(dialog_message), /*icon=*/nullptr);
    auto params = std::make_unique<ExtensionSettingsOverriddenDialog::Params>(
        extension->id(), preference_name, histogram_name,
        std::move(show_params));

    std::vector<IconFetchParams> icon_lookups;

    // Previous search engine before the extension overrode it.
    SettingsOverriddenDialogController::SettingOption& previous_setting =
        params->content.previous_setting.emplace();
    previous_setting.text = secondary_search.name;
    previous_setting.description = l10n_util::GetStringUTF16(
        IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_PREVIOUS_CHOICE);
    icon_lookups.emplace_back(secondary_search.favicon_url,
                              /*extension_name=*/std::string(),
                              &previous_setting.image);
    // New search engine from the overriding extension:
    SettingsOverriddenDialogController::SettingOption& new_setting =
        params->content.new_setting.emplace();
    new_setting.text = default_search->short_name();
    new_setting.description = l10n_util::GetStringFUTF16(
        IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_RECENT_CHANGE,
        extension_name_for_ui);
    icon_lookups.emplace_back(default_search->favicon_url(), extension->name(),
                              &new_setting.image);

    // Asynchronously look up icons (if needed) then continue.
    FetchIconsThenRun(
        icon_lookups, profile,
        base::BindOnce(std::move(done_callback), std::move(params)));
    return;
  }

  const gfx::VectorIcon* icon = nullptr;
  std::u16string dialog_title;
  switch (secondary_search.type) {
    case SecondarySearchInfo::Type::kGoogle:
      dialog_title = l10n_util::GetStringUTF16(
          IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_TITLE_BACK_TO_GOOGLE);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      icon = &vector_icons::kGoogleGLogoIcon;
#endif
      break;
    case SecondarySearchInfo::Type::kNonGoogleInDefaultList:
      DCHECK(!secondary_search.name.empty());
      dialog_title = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_TITLE_BACK_TO_OTHER,
          secondary_search.name);
      break;
    case SecondarySearchInfo::Type::kOther:
      dialog_title = l10n_util::GetStringUTF16(
          IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_TITLE_GENERIC);
      break;
  }
  std::u16string dialog_message = l10n_util::GetStringFUTF16(
      IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_BODY_GENERIC, formatted_search_url,
      extensions::ui_util::GetFixupExtensionNameForUIDisplay(
          extension->name()));

  SettingsOverriddenDialogController::ShowParams show_params(
      std::move(dialog_title), std::move(dialog_message), icon);
  auto params = std::make_unique<ExtensionSettingsOverriddenDialog::Params>(
      extension->id(), preference_name, histogram_name, std::move(show_params));

  // There are no async operations needed for the non-explicit-choice dialog,
  // so trigger the callback immediately.
  std::move(done_callback).Run(std::move(params));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

}  // namespace settings_overridden_params
