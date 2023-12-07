// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/settings_overridden_params_providers.h"

#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/extensions/controlled_home_bubble_delegate.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_url_handler.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings_overridden_params {

namespace {

// Returns the number of extensions that are currently enabled that override the
// default search setting.
size_t GetNumberOfExtensionsThatOverrideSearch(Profile* profile) {
  const auto* const registry = extensions::ExtensionRegistry::Get(profile);
  const auto overrides_search = [](auto extension) {
    auto* const settings = extensions::SettingsOverrides::Get(extension.get());
    return settings && settings->search_engine;
  };
  return base::ranges::count_if(registry->enabled_extensions(),
                                overrides_search);
}

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

  // The name of the search engine; only populated when |type| is
  // kNonGoogleInDefaultList.
  std::u16string name;
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

  if (num_overriding_extensions > 1) {
    // Another extension would take over.
    // NOTE(devlin): Theoretically, we could try and figure out exactly which
    // extension would take over, and include the origin of the secondary
    // search. However, this (>1 overriding extension) is an uncommon case, and
    // all that will happen is that we'll prompt the user that the new extension
    // is overriding search.
    return {SecondarySearchInfo::Type::kOther};
  }

  const TemplateURLService* const template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  const TemplateURL* const secondary_search =
      template_url_service->GetDefaultSearchProviderIgnoringExtensions();
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
  if (google_util::IsGoogleSearchUrl(search_url))
    return {SecondarySearchInfo::Type::kGoogle, origin};

  if (!template_url_service->ShowInDefaultList(secondary_search)) {
    // Found another search engine, but it's not one of the default options.
    return {SecondarySearchInfo::Type::kOther, origin};
  }

  // The secondary search engine is another of the defaults.
  return {SecondarySearchInfo::Type::kNonGoogleInDefaultList, origin,
          secondary_search->short_name()};
}

}  // namespace

std::optional<ExtensionSettingsOverriddenDialog::Params> GetNtpOverriddenParams(
    Profile* profile) {
  const GURL ntp_url(chrome::kChromeUINewTabURL);
  const extensions::Extension* extension =
      ExtensionWebUI::GetExtensionControllingURL(ntp_url, profile);
  if (!extension)
    return std::nullopt;

  // This preference tracks whether users have acknowledged the extension's
  // control, so that they are not warned twice about the same extension.
  const char* preference_name = extensions::kNtpOverridingExtensionAcknowledged;

  std::vector<GURL> possible_rewrites =
      content::BrowserURLHandler::GetInstance()->GetPossibleRewrites(ntp_url,
                                                                     profile);
  // We already know that the extension is the primary NTP controller.
  DCHECK(!possible_rewrites.empty());
  DCHECK_EQ(extension->url().host_piece(), possible_rewrites[0].host_piece())
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
      base::UTF8ToUTF16(extension->name().c_str()));

  return ExtensionSettingsOverriddenDialog::Params(
      extension->id(), preference_name, histogram_name, std::move(dialog_title),
      std::move(dialog_message), icon);
}

std::optional<ExtensionSettingsOverriddenDialog::Params>
GetSearchOverriddenParams(Profile* profile) {
  const extensions::Extension* extension =
      extensions::GetExtensionOverridingSearchEngine(profile);
  if (!extension)
    return std::nullopt;

  // For historical reasons, the search override preference is the same as the
  // one we use for the controlled home setting. We continue this so that
  // users won't see the bubble or dialog UI if they've already acknowledged
  // an older version.
  const char* preference_name =
      ControlledHomeBubbleDelegate::kAcknowledgedPreference;

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
    return std::nullopt;
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
  const gfx::VectorIcon* icon = nullptr;
  std::u16string dialog_title;
  switch (secondary_search.type) {
    case SecondarySearchInfo::Type::kGoogle:
      histogram_name = kBackToGoogleHistogramName;
      dialog_title = l10n_util::GetStringUTF16(
          IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_TITLE_BACK_TO_GOOGLE);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      icon = &vector_icons::kGoogleGLogoIcon;
#endif
      break;
    case SecondarySearchInfo::Type::kNonGoogleInDefaultList:
      DCHECK(!secondary_search.name.empty());
      histogram_name = kBackToOtherHistogramName;
      dialog_title = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_TITLE_BACK_TO_OTHER,
          secondary_search.name);
      break;
    case SecondarySearchInfo::Type::kOther:
      histogram_name = kGenericDialogHistogramName;
      dialog_title = l10n_util::GetStringUTF16(
          IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_TITLE_GENERIC);
      break;
  }
  std::u16string dialog_message = l10n_util::GetStringFUTF16(
      IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_BODY_GENERIC, formatted_search_url,
      base::UTF8ToUTF16(extension->name().c_str()));

  return ExtensionSettingsOverriddenDialog::Params(
      extension->id(), preference_name, histogram_name, std::move(dialog_title),
      std::move(dialog_message), icon);
}

}  // namespace settings_overridden_params
