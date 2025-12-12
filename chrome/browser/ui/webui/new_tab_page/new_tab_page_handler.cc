// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/new_tab_page/feature_promo_helper/new_tab_page_feature_promo_helper.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_factory.h"
#include "chrome/browser/new_tab_page/modules/modules_constants.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/new_tab_page/promos/promo_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/promos/promos_pref_names.h"
#include "chrome/browser/promos/promos_utils.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_controller.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/customize_chrome_utils.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_helper.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_pref_names.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui/webui_util_desktop.h"
#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/ntp_tiles/tile_type.h"
#include "components/omnibox/browser/omnibox.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_provider_logos/logo_service.h"
#include "components/search_provider_logos/switches.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/sync/service/sync_service.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "ui/native_theme/native_theme.h"

namespace {

const int64_t kMaxDownloadBytes = 1024 * 1024;

constexpr char kDisableInteraction[] = "disable";
constexpr char kDismissInteraction[] = "dismiss";
constexpr char kIgnoreInteraction[] = "ignore";
constexpr char kUseInteraction[] = "use";
constexpr auto kModuleInteractionNames =
    base::MakeFixedFlatSet<std::string_view>(
        {kDisableInteraction, kDismissInteraction, kIgnoreInteraction,
         kUseInteraction});

// Returns a list of module IDs that are eligible for HATS.
std::vector<std::string> GetSurveyEligibleModuleIds() {
  return base::SplitString(
      base::GetFieldTrialParamValueByFeature(
          features::kHappinessTrackingSurveysForDesktopNtpModules,
          ntp_features::kNtpModulesEligibleForHappinessTrackingSurveyParam),
      ",:;", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
}

// Returns true if we should force dark foreground colors for the Google logo
// and the One Google Bar. This is done to fix specific GWS themes where the
// always-light logo and OGB colors do not sufficiently contrast with lighter
// image backgrounds (see crbug.com/1329552).
// TODO(crbug.com/40842305): Address this in a general way and extend support to
// custom background images, not just CWS themes.
bool ShouldForceDarkForegroundColorsForLogo(const ThemeService* theme_service) {
  const auto* theme_supplier = theme_service->GetThemeSupplier();
  if (!theme_supplier || theme_supplier->get_theme_type() !=
                             ui::ColorProviderKey::ThemeInitializerSupplier::
                                 ThemeType::kExtension) {
    return false;
  }
  static constexpr auto kPrideThemeExtensionIdsDarkForeground =
      base::MakeFixedFlatSet<std::string_view>({
          "klnkeldihpjnjoopojllmnpepbpljico",
          "iffdmpenldeofnlfjmbjcdmafhoekmka",
          "mckialangcdpcdcflekinnpamfkmkobo",
      });

  const std::string& extension_id = theme_supplier->extension_id();
  return base::Contains(kPrideThemeExtensionIdsDarkForeground, extension_id);
}

new_tab_page::mojom::ThemePtr MakeTheme(
    const ui::ColorProvider& color_provider,
    const ui::ThemeProvider* theme_provider,
    ThemeService* theme_service,
    NtpCustomBackgroundService* ntp_custom_background_service,
    content::WebContents* web_contents) {
  if (ntp_custom_background_service) {
    ntp_custom_background_service->RefreshBackgroundIfNeeded();
  }
  auto theme = new_tab_page::mojom::Theme::New();
  auto most_visited = most_visited::mojom::MostVisitedTheme::New();
  auto custom_background =
      ntp_custom_background_service
          ? ntp_custom_background_service->GetCustomBackground()
          : std::nullopt;
  theme->background_color = color_provider.GetColor(kColorNewTabPageBackground);
  theme->is_baseline = theme_service->GetIsBaseline();
  const bool theme_has_custom_image =
      theme_provider->HasCustomImage(IDR_THEME_NTP_BACKGROUND);
  SkColor text_color;
  if (custom_background.has_value()) {
    text_color = color_provider.GetColor(kColorNewTabPageTextUnthemed);
    theme->logo_color =
        color_provider.GetColor(kColorNewTabPageLogoUnthemedLight);
    most_visited->background_color =
        color_provider.GetColor(kColorNewTabPageMostVisitedTileBackground);
  } else if (theme_provider->HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    text_color = color_provider.GetColor(kColorNewTabPageTextUnthemed);
    theme->logo_color =
        color_provider.GetColor(kColorNewTabPageLogoUnthemedLight);
    most_visited->background_color =
        color_provider.GetColor(kColorNewTabPageMostVisitedTileBackground);
  } else {
    text_color = color_provider.GetColor(kColorNewTabPageText);
    if (theme_provider->GetDisplayProperty(
            ThemeProperties::NTP_LOGO_ALTERNATE) == 1 ||
        (!theme_service->GetIsGrayscale() &&
         theme_service->GetUserColor().has_value())) {
      theme->logo_color = color_provider.GetColor(kColorNewTabPageLogo);
    }

    most_visited->background_color =
        color_provider.GetColor(kColorNewTabPageMostVisitedTileBackground);
  }

  most_visited->use_white_tile_icon =
      color_utils::IsDark(most_visited->background_color);
  most_visited->is_dark = !color_utils::IsDark(text_color);
  theme->text_color = text_color;
  theme->is_dark =
      !color_utils::IsDark(color_provider.GetColor(kColorNewTabPageText));
  auto background_image = new_tab_page::mojom::BackgroundImage::New();
  if (theme_has_custom_image) {
    if (theme_service->UsingExtensionTheme()) {
      background_image->image_source =
          new_tab_page::mojom::NtpBackgroundImageSource::kThirdPartyTheme;
    }
    theme->is_custom_background = false;
    auto theme_id = theme_service->GetThemeID();
    background_image->url = GURL(base::StrCat(
        {"chrome-untrusted://theme/IDR_THEME_NTP_BACKGROUND?", theme_id}));
    background_image->url_2x = GURL(base::StrCat(
        {"chrome-untrusted://theme/IDR_THEME_NTP_BACKGROUND@2x?", theme_id}));
    if (theme_provider->HasCustomImage(IDR_THEME_NTP_ATTRIBUTION)) {
      background_image->attribution_url = GURL(base::StrCat(
          {"chrome://theme/IDR_THEME_NTP_ATTRIBUTION?", theme_id}));
    }
    background_image->size = "initial";
    switch (theme_provider->GetDisplayProperty(
        ThemeProperties::NTP_BACKGROUND_TILING)) {
      case ThemeProperties::NO_REPEAT:
        background_image->repeat_x = "no-repeat";
        background_image->repeat_y = "no-repeat";
        break;
      case ThemeProperties::REPEAT_X:
        background_image->repeat_x = "repeat";
        background_image->repeat_y = "no-repeat";
        break;
      case ThemeProperties::REPEAT_Y:
        background_image->repeat_x = "no-repeat";
        background_image->repeat_y = "repeat";
        break;
      case ThemeProperties::REPEAT:
        background_image->repeat_x = "repeat";
        background_image->repeat_y = "repeat";
        break;
    }
    int alignment = theme_provider->GetDisplayProperty(
        ThemeProperties::NTP_BACKGROUND_ALIGNMENT);
    if (alignment & ThemeProperties::ALIGN_LEFT) {
      background_image->position_x = "left";
    } else if (alignment & ThemeProperties::ALIGN_RIGHT) {
      background_image->position_x = "right";
    } else {
      background_image->position_x = "center";
    }
    if (alignment & ThemeProperties::ALIGN_TOP) {
      background_image->position_y = "top";
    } else if (alignment & ThemeProperties::ALIGN_BOTTOM) {
      background_image->position_y = "bottom";
    } else {
      background_image->position_y = "center";
    }
  } else if (custom_background.has_value()) {
    theme->is_custom_background = true;
    background_image->url = custom_background->custom_background_url;
    new_tab_page::mojom::NtpBackgroundImageSource image_source = new_tab_page::
        mojom::NtpBackgroundImageSource::kFirstPartyThemeWithoutDailyRefresh;
    if (custom_background->daily_refresh_enabled) {
      image_source = new_tab_page::mojom::NtpBackgroundImageSource::
          kFirstPartyThemeWithDailyRefresh;
    } else if (custom_background->local_background_id.has_value()) {
      if (custom_background->is_inspiration_image) {
        image_source = new_tab_page::mojom::NtpBackgroundImageSource::
            kWallpaperSearchInspiration;
      } else {
        image_source =
            new_tab_page::mojom::NtpBackgroundImageSource::kWallpaperSearch;
      }
    } else if (custom_background->is_uploaded_image) {
      image_source =
          new_tab_page::mojom::NtpBackgroundImageSource::kUploadedImage;
    }
    background_image->image_source = image_source;
  } else {
    background_image = nullptr;
  }

  // The special case handling that forces a dark Google logo should only be
  // applied when the user does not have a custom background selected and has
  // installed a CWS theme with a bundled background image. The first condition
  // is necessary as a custom background image can be set while a CWS theme with
  // a bundled image is concurrently enabled (see crbug.com/1329552).
  if (!custom_background.has_value() && theme_has_custom_image &&
      ShouldForceDarkForegroundColorsForLogo(theme_service)) {
    theme->logo_color =
        color_provider.GetColor(kColorNewTabPageLogoUnthemedDark);
  }

  theme->background_image = std::move(background_image);
  if (custom_background.has_value() && !theme_has_custom_image) {
    theme->background_image_attribution_1 =
        custom_background->custom_background_attribution_line_1;
    theme->background_image_attribution_2 =
        custom_background->custom_background_attribution_line_2;
    theme->background_image_attribution_url =
        custom_background->custom_background_attribution_action_url;
    theme->background_image_collection_id = custom_background->collection_id;
    theme->daily_refresh_enabled = custom_background->daily_refresh_enabled;
  }

  theme->most_visited = std::move(most_visited);

  return theme;
}

SkColor ParseHexColor(const std::string& color) {
  SkColor result;
  if (color.size() == 7 && color[0] == '#' &&
      base::HexStringToUInt(color.substr(1), &result)) {
    return SkColorSetA(result, SK_AlphaOPAQUE);
  }
  return SK_ColorTRANSPARENT;
}

new_tab_page::mojom::ImageDoodlePtr MakeImageDoodle(
    search_provider_logos::LogoType type,
    const std::string& data,
    const std::string& mime_type,
    const GURL& animated_url,
    int width_px,
    int height_px,
    const std::string& background_color,
    GURL log_url,
    GURL cta_log_url) {
  auto doodle = new_tab_page::mojom::ImageDoodle::New();
  std::string base64 = base::Base64Encode(data);
  doodle->image_url = GURL(base::StringPrintf(
      "data:%s;base64,%s", mime_type.c_str(), base64.c_str()));
  if (type == search_provider_logos::LogoType::ANIMATED) {
    doodle->animation_url = animated_url;
  }
  doodle->width = width_px;
  doodle->height = height_px;
  doodle->background_color = ParseHexColor(background_color);
  if (type == search_provider_logos::LogoType::ANIMATED) {
    doodle->image_impression_log_url = cta_log_url;
    doodle->animation_impression_log_url = log_url;
  } else {
    doodle->image_impression_log_url = log_url;
  }
  return doodle;
}

new_tab_page::mojom::PromoPtr MakePromo(const PromoData& data) {
  // |data.middle_slot_json| is safe to be decoded here. The JSON string is part
  // of a larger JSON initially decoded using the data decoder utility in the
  // PromoService to base::Value. The middle-slot promo part is then reencoded
  // from base::Value to a JSON string stored in |data.middle_slot_json|.
  auto middle_slot = base::JSONReader::Read(
      data.middle_slot_json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!middle_slot.has_value()) {
    return nullptr;
  }

  base::Value::Dict& middle_slot_dict = middle_slot->GetDict();
  if (middle_slot_dict.FindBoolByDottedPath("hidden").value_or(false)) {
    return nullptr;
  }

  auto promo = new_tab_page::mojom::Promo::New();
  promo->id = data.promo_id;
  auto* parts = middle_slot_dict.FindList("part");
  if (parts) {
    std::vector<new_tab_page::mojom::PromoPartPtr> mojom_parts;
    for (const base::Value& part : *parts) {
      const base::Value::Dict& part_dict = part.GetDict();
      if (part_dict.Find("image")) {
        auto mojom_image = new_tab_page::mojom::PromoImagePart::New();
        auto* image_url = part_dict.FindStringByDottedPath("image.image_url");
        if (!image_url || image_url->empty()) {
          continue;
        }
        mojom_image->image_url = GURL(*image_url);
        auto* target = part_dict.FindStringByDottedPath("image.target");
        if (target && !target->empty()) {
          mojom_image->target = GURL(*target);
        }
        mojom_parts.push_back(
            new_tab_page::mojom::PromoPart::NewImage(std::move(mojom_image)));
      } else if (part_dict.Find("link")) {
        auto mojom_link = new_tab_page::mojom::PromoLinkPart::New();
        auto* url = part_dict.FindStringByDottedPath("link.url");
        if (!url || url->empty()) {
          continue;
        }
        mojom_link->url = GURL(*url);
        auto* text = part_dict.FindStringByDottedPath("link.text");
        if (!text || text->empty()) {
          continue;
        }
        mojom_link->text = *text;
        mojom_parts.push_back(
            new_tab_page::mojom::PromoPart::NewLink(std::move(mojom_link)));
      } else if (part_dict.Find("text")) {
        auto mojom_text = new_tab_page::mojom::PromoTextPart::New();
        auto* text = part_dict.FindStringByDottedPath("text.text");
        if (!text || text->empty()) {
          continue;
        }
        mojom_text->text = *text;
        mojom_parts.push_back(
            new_tab_page::mojom::PromoPart::NewText(std::move(mojom_text)));
      }
    }
    promo->middle_slot_parts = std::move(mojom_parts);
  }
  promo->log_url = data.promo_log_url;
  return promo;
}

base::Value::Dict MakeModuleInteractionTriggerIdDictionary() {
  const auto data = base::GetFieldTrialParamValueByFeature(
      features::kHappinessTrackingSurveysForDesktopNtpModules,
      ntp_features::kNtpModulesInteractionBasedSurveyEligibleIdsParam);
  if (data.empty()) {
    return base::Value::Dict();
  }

  auto value_with_error = base::JSONReader::ReadAndReturnValueWithError(
      data, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  if (!value_with_error.has_value()) {
    LOG(ERROR)
        << "Failed to parse "
           "ntp_features::kNtpModulesInteractionBasedSurveyEligibleIdsParam ("
        << value_with_error.error().message << ") on line "
        << value_with_error.error().line << " at position "
        << value_with_error.error().column;
    return base::Value::Dict();
  }

  if (!value_with_error->is_dict()) {
    LOG(WARNING)
        << "ntp_features::kNtpModulesInteractionBasedSurveyEligibleIdsParam "
           "data skipped. Not a dictionary.";
    return base::Value::Dict();
  }

  return std::move(*value_with_error).TakeDict();
}

}  // namespace

// static
const char NewTabPageHandler::kModuleDismissedHistogram[] =
    "NewTabPage.Modules.Dismissed";
const char NewTabPageHandler::kModuleRestoredHistogram[] =
    "NewTabPage.Modules.Restored";

NewTabPageHandler::NewTabPageHandler(
    mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<new_tab_page::mojom::Page> pending_page,
    Profile* profile,
    NtpCustomBackgroundService* ntp_custom_background_service,
    ThemeService* theme_service,
    search_provider_logos::LogoService* logo_service,
    syncer::SyncService* sync_service,
    segmentation_platform::SegmentationPlatformService*
        segmentation_platform_service,
    content::WebContents* web_contents,
    const base::Time& ntp_navigation_start_time,
    const std::vector<ntp::ModuleIdDetail>* module_id_details)
    : SettingsEnabledObserver(
          optimization_guide::UserVisibleFeatureKey::kWallpaperSearch),
      ntp_custom_background_service_(ntp_custom_background_service),
      logo_service_(logo_service),
      theme_provider_(webui::GetThemeProviderDeprecated(web_contents)),
      theme_service_(theme_service),
      sync_service_(sync_service),
      segmentation_platform_service_(segmentation_platform_service),
      profile_(profile),
      web_contents_(web_contents),
      feature_promo_helper_(std::make_unique<NewTabPageFeaturePromoHelper>()),
      ntp_navigation_start_time_(ntp_navigation_start_time),
      module_id_details_(module_id_details),
      logger_(profile,
              GURL(chrome::kChromeUINewTabPageURL),
              ntp_navigation_start_time),
      promo_service_(PromoServiceFactory::GetForProfile(profile)),
      microsoft_auth_service_(
          MicrosoftAuthServiceFactory::GetForProfile(profile)),
      interaction_module_id_trigger_dict_(
          MakeModuleInteractionTriggerIdDictionary()),
      browser_window_changed_subscription_(
          webui::RegisterBrowserWindowInterfaceChanged(
              web_contents_,
              base::BindRepeating(
                  &NewTabPageHandler::OnBrowserWindowInterfaceChanged,
                  base::Unretained(this)))),
      page_{std::move(pending_page)},
      receiver_{this, std::move(pending_page_handler)} {
  CHECK(ntp_custom_background_service_);
  CHECK(logo_service_);
  CHECK(theme_service_);
  CHECK(promo_service_);
  CHECK(web_contents_);
  CHECK(feature_promo_helper_);
  native_theme_observation_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
  theme_service_observation_.Observe(theme_service_.get());
  ntp_custom_background_service_observation_.Observe(
      ntp_custom_background_service_.get());
  promo_service_observation_.Observe(promo_service_.get());
  if (customize_chrome::IsWallpaperSearchEnabledForProfile(profile_)) {
    optimization_guide_keyed_service_ =
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
    if (optimization_guide_keyed_service_) {
      optimization_guide_keyed_service_
          ->AddModelExecutionSettingsEnabledObserver(this);
    }
  }

  if (microsoft_auth_service_) {
    microsoft_auth_service_->AddObserver(this);
  }

  OnBrowserWindowInterfaceChanged();

  if (base::FeatureList::IsEnabled(
          ntp_features::kNtpBackgroundImageErrorDetection)) {
    ntp_custom_background_service_->VerifyCustomBackgroundImageURL();
  } else {
    OnThemeChanged();
  }

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kNtpModulesVisible,
      base::BindRepeating(&NewTabPageHandler::UpdateDisabledModules,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kNtpDisabledModules,
      base::BindRepeating(&NewTabPageHandler::UpdateDisabledModules,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kNtpHiddenModules,
      base::BindRepeating(&NewTabPageHandler::UpdateDisabledModules,
                          base::Unretained(this)));

  pref_change_registrar_.Add(
      prefs::kSeedColorChangeCount,
      base::BindRepeating(&NewTabPageHandler::MaybeShowWebstoreToast,
                          base::Unretained(this)));

  pref_change_registrar_.Add(
      prefs::kNtpToolChipsVisible,
      base::BindRepeating(&NewTabPageHandler::UpdateActionChipsVisibility,
                          base::Unretained(this)));
}

NewTabPageHandler::~NewTabPageHandler() {
  if (optimization_guide_keyed_service_) {
    optimization_guide_keyed_service_
        ->RemoveModelExecutionSettingsEnabledObserver(this);
    optimization_guide_keyed_service_ = nullptr;
  }
  if (microsoft_auth_service_) {
    microsoft_auth_service_->RemoveObserver(this);
  }
}

// static
void NewTabPageHandler::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kNtpDisabledModules);
  registry->RegisterListPref(prefs::kNtpHiddenModules);
  registry->RegisterListPref(prefs::kNtpModulesOrder);
  registry->RegisterBooleanPref(prefs::kNtpModulesVisible, true);
  registry->RegisterBooleanPref(prefs::kNtpToolChipsVisible, true);
  registry->RegisterIntegerPref(prefs::kNtpCustomizeChromeButtonOpenCount, 0);
  registry->RegisterDictionaryPref(prefs::kNtpModulesInteractedCountDict);
  registry->RegisterDictionaryPref(prefs::kNtpModulesLoadedCountDict);
  registry->RegisterIntegerPref(prefs::kNtpWallpaperSearchButtonShownCount, 0);
  registry->RegisterBooleanPref(prefs::kNtpOutlookModuleVisible, false);
  registry->RegisterBooleanPref(prefs::kNtpSharepointModuleVisible, false);
  registry->RegisterIntegerPref(prefs::kNtpComposeButtonShownCountPrefName, 0);
  registry->RegisterIntegerPref(
      prefs::kNtpCustomizeChromeSidePanelAutoOpeningsCount, 0);
  registry->RegisterBooleanPref(prefs::kNtpCustomizeChromeExplicitlyClosed,
                                false);
  registry->RegisterBooleanPref(prefs::kNtpCustomizeChromeIPHAutoOpened, false);
}

void NewTabPageHandler::SetMostVisitedSettings(ntp_tiles::TileType type,
                                               bool visible) {
  bool old_visible = IsShortcutsVisible();
  if (old_visible != visible) {
    profile_->GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, visible);
    logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_TOGGLE_VISIBILITY,
                     base::TimeDelta() /* unused */);
  }

  ntp_tiles::TileType old_type = GetTileType();
  if (old_type != type) {
    profile_->GetPrefs()->SetBoolean(ntp_prefs::kNtpCustomLinksVisible,
                                     type == ntp_tiles::TileType::kCustomLinks);
    profile_->GetPrefs()->SetBoolean(
        ntp_prefs::kNtpEnterpriseShortcutsVisible,
        type == ntp_tiles::TileType::kEnterpriseShortcuts);
    logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_TOGGLE_TYPE,
                     base::TimeDelta() /* unused */);
  }
}

void NewTabPageHandler::GetMostVisitedSettings(
    GetMostVisitedSettingsCallback callback) {
  ntp_tiles::TileType type = GetTileType();
  bool visible = IsShortcutsVisible();
  std::move(callback).Run(type, visible);
}

void NewTabPageHandler::GetDoodle(GetDoodleCallback callback) {
  search_provider_logos::LogoCallbacks callbacks;
  callbacks.on_cached_encoded_logo_available =
      base::BindOnce(&NewTabPageHandler::OnLogoAvailable,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  // This will trigger re-downloading the doodle and caching it. This means a
  // new doodle will be returned on subsequent NTP loads.
  logo_service_->GetLogo(std::move(callbacks), /*for_webui_ntp=*/true);
}

void NewTabPageHandler::UpdatePromoData() {
  if (promo_service_->promo_data().has_value()) {
    OnPromoDataUpdated();
  }
  promo_load_start_time_ = base::TimeTicks::Now();
  promo_service_->Refresh();
}

void NewTabPageHandler::BlocklistPromo(const std::string& promo_id) {
  promo_service_->BlocklistPromo(promo_id);
}

void NewTabPageHandler::UndoBlocklistPromo(const std::string& promo_id) {
  promo_service_->UndoBlocklistPromo(promo_id);
}

void NewTabPageHandler::OnDismissModule(const std::string& module_id) {
  const std::string histogram_prefix(kModuleDismissedHistogram);
  base::UmaHistogramExactLinear(histogram_prefix, 1, 1);
  base::UmaHistogramExactLinear(histogram_prefix + "." + module_id, 1, 1);

  RecordModuleInteraction(module_id);
  MaybeLaunchInteractionSurvey(kDismissInteraction, module_id);
}

void NewTabPageHandler::OnRestoreModule(const std::string& module_id) {
  const std::string histogram_prefix(kModuleRestoredHistogram);
  base::UmaHistogramExactLinear(histogram_prefix, 1, 1);
  base::UmaHistogramExactLinear(histogram_prefix + "." + module_id, 1, 1);
}

void NewTabPageHandler::SetModulesVisible(bool visible) {
  DisableModuleAutoRemoval(profile_, ntp_modules::kAllModulesId);
  profile_->GetPrefs()->SetBoolean(prefs::kNtpModulesVisible, visible);
}

void NewTabPageHandler::SetModuleDisabled(const std::string& module_id,
                                          bool disabled) {
  ScopedListPrefUpdate update(profile_->GetPrefs(), prefs::kNtpDisabledModules);
  base::Value::List& list = update.Get();
  base::Value module_id_value(module_id);
  if (disabled) {
    if (!base::Contains(list, module_id_value)) {
      list.Append(std::move(module_id_value));
    }
  } else {
    list.EraseValue(module_id_value);
  }

  RecordModuleInteraction(module_id);
  MaybeLaunchInteractionSurvey(kDisableInteraction, module_id);
}

void NewTabPageHandler::UpdateDisabledModules() {
  std::set<std::string> module_ids_set;
  // If the module visibility is managed by policy we either disable all modules
  // (if invisible) or no modules (if visible).
  if (!profile_->GetPrefs()->IsManagedPreference(prefs::kNtpModulesVisible)) {
    const auto& disabled_module_ids_value =
        profile_->GetPrefs()->GetList(prefs::kNtpDisabledModules);
    for (const auto& id : disabled_module_ids_value) {
      module_ids_set.insert(id.GetString());
    }
  }

  // Hidden modules should be respected as long as modules are visible.
  if (profile_->GetPrefs()->GetBoolean(prefs::kNtpModulesVisible)) {
    const auto& hidden_module_ids_value =
        profile_->GetPrefs()->GetList(prefs::kNtpHiddenModules);
    for (const auto& id : hidden_module_ids_value) {
      module_ids_set.insert(id.GetString());
    }
  }

  std::vector<std::string> module_ids(module_ids_set.begin(),
                                      module_ids_set.end());
  page_->SetDisabledModules(
      !profile_->GetPrefs()->GetBoolean(prefs::kNtpModulesVisible),
      std::move(module_ids));
}

void NewTabPageHandler::OnModulesLoadedWithData(
    const std::vector<std::string>& module_ids) {
  UpdateModulesStaleness(profile_, module_ids);

  for (const auto& module_id : module_ids) {
    IncrementDictPrefKeyCount(prefs::kNtpModulesLoadedCountDict, module_id);
  }

  std::vector<std::string> survey_eligible_module_ids =
      GetSurveyEligibleModuleIds();
  if (std::any_of(module_ids.begin(), module_ids.end(),
                  [&survey_eligible_module_ids](std::string id) {
                    return base::Contains(survey_eligible_module_ids, id);
                  })) {
    HatsService* hats_service = HatsServiceFactory::GetForProfile(
        profile_, /*create_if_necessary=*/true);
    CHECK(hats_service);
    hats_service->LaunchDelayedSurveyForWebContents(
        kHatsSurveyTriggerNtpModules, web_contents_, 0);
    return;
  }

  const auto module_ignored_criteria_threshold =
      base::GetFieldTrialParamByFeatureAsInt(
          features::kHappinessTrackingSurveysForDesktopNtpModules,
          ntp_features::kNtpModuleIgnoredCriteriaThreshold, 25);
  for (const auto& module_id : module_ids) {
    const base::Value::Dict& interacted_counts_dict =
        profile_->GetPrefs()->GetDict(prefs::kNtpModulesInteractedCountDict);
    std::optional<int> interacted_count =
        interacted_counts_dict.FindInt(module_id);
    if (interacted_count.value_or(0) != 0) {
      continue;
    }

    const base::Value::Dict& loaded_counts_dict =
        profile_->GetPrefs()->GetDict(prefs::kNtpModulesLoadedCountDict);
    std::optional<int> loaded_count = loaded_counts_dict.FindInt(module_id);
    if (loaded_count.value_or(0) >= module_ignored_criteria_threshold) {
      const auto survey_delay_time_ms = base::GetFieldTrialParamByFeatureAsInt(
          features::kHappinessTrackingSurveysForDesktopNtpModules,
          ntp_features::kNtpModuleIgnoredHaTSDelayTimeParam, 0);
      MaybeLaunchInteractionSurvey(kIgnoreInteraction, module_id,
                                   survey_delay_time_ms);
      break;
    }
  }
}

void NewTabPageHandler::OnModuleUsed(const std::string& module_id) {
  RecordModuleInteraction(module_id);
  MaybeLaunchInteractionSurvey(kUseInteraction, module_id);
}

void NewTabPageHandler::GetModulesIdNames(GetModulesIdNamesCallback callback) {
  std::vector<new_tab_page::mojom::ModuleIdNamePtr> modules_details;
  for (const auto& module_id_detail : *module_id_details_) {
    auto module_id_name = new_tab_page::mojom::ModuleIdName::New();
    module_id_name->id = module_id_detail.id_;
    module_id_name->name =
        l10n_util::GetStringUTF8(module_id_detail.name_message_id_);
    modules_details.push_back(std::move(module_id_name));
  }

  std::move(callback).Run(std::move(modules_details));
}

void NewTabPageHandler::SetModulesOrder(
    const std::vector<std::string>& module_ids) {
  base::Value::List module_ids_value;
  for (const auto& module_id : module_ids) {
    module_ids_value.Append(module_id);
  }
  profile_->GetPrefs()->SetList(prefs::kNtpModulesOrder,
                                std::move(module_ids_value));
}

void NewTabPageHandler::GetModulesOrder(GetModulesOrderCallback callback) {
  std::vector<std::string> module_ids;

  // First, apply order as set by the last drag&drop interaction.
  if (base::FeatureList::IsEnabled(ntp_features::kNtpModulesDragAndDrop)) {
    const auto& module_ids_value =
        profile_->GetPrefs()->GetList(prefs::kNtpModulesOrder);
    for (const auto& id : module_ids_value) {
      module_ids.push_back(id.GetString());
    }
  }

  // Second, append Finch order for modules _not_ ordered by drag&drop.
  std::ranges::copy_if(ntp_features::GetModulesOrder(),
                       std::back_inserter(module_ids),
                       [&module_ids](const std::string& id) {
                         return !base::Contains(module_ids, id);
                       });

  // Third, append default module order for any modules not ordered by
  // drag&drop or Finch.
  std::ranges::copy_if(ntp_modules::kOrderedModuleIds,
                       std::back_inserter(module_ids),
                       [&module_ids](const std::string& id) {
                         return !base::Contains(module_ids, id);
                       });

  std::move(callback).Run(std::move(module_ids));
}

void NewTabPageHandler::UpdateModulesLoadable() {
  if (!microsoft_auth_service_ || SyncMicrosoftModulesWithAuth()) {
    page_->SetModulesLoadable();
  }
}

void NewTabPageHandler::UpdateActionChipsVisibility() {
  page_->SetActionChipsVisibility(IsActionChipsVisible());
}

void NewTabPageHandler::UpdateFooterVisibility() {
  if (!base::FeatureList::IsEnabled(ntp_features::kNtpFooter)) {
    return;
  }

  // TODO(webium): FooterController depends on BrowserView, but WebUIBrowser
  // doesn't have a BrowserView.
  if (webui_browser::IsWebUIBrowserEnabled()) {
    return;
  }

  auto* browser = webui::GetBrowserWindowInterface(web_contents_);
  if (!browser) {
    // TODO(crbug.com/378475391): NTP should always load into a WebContents
    // owned by a TabModel. Remove this once NTP loading has been restricted to
    // browser tabs only.
    return;
  }

  auto* footer_controller = browser->GetFeatures().new_tab_footer_controller();
  CHECK(footer_controller);
  OnFooterVisibilityUpdated(footer_controller->GetFooterVisible(web_contents_));
}

void NewTabPageHandler::OnAppRendered(double time) {
  logger_.LogEvent(NTP_APP_RENDERED,
                   base::Time::FromMillisecondsSinceUnixEpoch(time) -
                       ntp_navigation_start_time_);
}

void NewTabPageHandler::OnOneGoogleBarRendered(double time) {
  logger_.LogEvent(NTP_ONE_GOOGLE_BAR_SHOWN,
                   base::Time::FromMillisecondsSinceUnixEpoch(time) -
                       ntp_navigation_start_time_);
}

void NewTabPageHandler::OnPromoRendered(double time,
                                        const std::optional<GURL>& log_url) {
  logger_.LogEvent(NTP_MIDDLE_SLOT_PROMO_SHOWN,
                   base::Time::FromMillisecondsSinceUnixEpoch(time) -
                       ntp_navigation_start_time_);
  if (log_url.has_value() && log_url->is_valid()) {
    Fetch(*log_url, base::NullCallback());
  }
}

void NewTabPageHandler::OnCustomizeDialogAction(
    new_tab_page::mojom::CustomizeDialogAction action) {
  NTPLoggingEventType event;
  switch (action) {
    case new_tab_page::mojom::CustomizeDialogAction::kCancelClicked:
      event = NTP_CUSTOMIZATION_MENU_CANCEL;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::kDoneClicked:
      event = NTP_CUSTOMIZATION_MENU_DONE;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::kOpenClicked:
      event = NTP_CUSTOMIZATION_MENU_OPENED;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::kBackgroundsBackClicked:
      event = NTP_BACKGROUND_BACK_CLICK;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kBackgroundsNoBackgroundSelected:
      event = NTP_BACKGROUND_DEFAULT_SELECTED;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kBackgroundsCollectionOpened:
      event = NTP_BACKGROUND_OPEN_COLLECTION;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kBackgroundsRefreshToggleClicked:
      event = NTP_BACKGROUND_REFRESH_TOGGLE_CLICKED;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::kBackgroundsImageSelected:
      event = NTP_BACKGROUND_SELECT_IMAGE;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kBackgroundsUploadFromDeviceClicked:
      event = NTP_BACKGROUND_UPLOAD_FROM_DEVICE;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kShortcutsCustomLinksClicked:
      event = NTP_CUSTOMIZE_SHORTCUT_CUSTOM_LINKS_CLICKED;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kShortcutsMostVisitedClicked:
      event = NTP_CUSTOMIZE_SHORTCUT_MOST_VISITED_CLICKED;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kShortcutsVisibilityToggleClicked:
      event = NTP_CUSTOMIZE_SHORTCUT_VISIBILITY_TOGGLE_CLICKED;
      break;
    default:
      NOTREACHED();
  }
  LogEvent(event);
}

void NewTabPageHandler::OnDoodleImageClicked(
    new_tab_page::mojom::DoodleImageType type,
    const std::optional<::GURL>& log_url) {
  NTPLoggingEventType event;
  switch (type) {
    case new_tab_page::mojom::DoodleImageType::kAnimation:
      event = NTP_ANIMATED_LOGO_CLICKED;
      break;
    case new_tab_page::mojom::DoodleImageType::kCta:
      event = NTP_CTA_LOGO_CLICKED;
      break;
    case new_tab_page::mojom::DoodleImageType::kStatic:
      event = NTP_STATIC_LOGO_CLICKED;
      break;
    default:
      NOTREACHED();
  }
  LogEvent(event);

  if (type == new_tab_page::mojom::DoodleImageType::kCta &&
      log_url.has_value()) {
    // We just ping the server to indicate a CTA image has been clicked.
    Fetch(*log_url, base::NullCallback());
  }
}

void NewTabPageHandler::OnDoodleImageRendered(
    new_tab_page::mojom::DoodleImageType type,
    double time,
    const GURL& log_url,
    OnDoodleImageRenderedCallback callback) {
  if (type == new_tab_page::mojom::DoodleImageType::kCta ||
      type == new_tab_page::mojom::DoodleImageType::kStatic) {
    logger_.LogEvent(type == new_tab_page::mojom::DoodleImageType::kCta
                         ? NTP_CTA_LOGO_SHOWN_FROM_CACHE
                         : NTP_STATIC_LOGO_SHOWN_FROM_CACHE,
                     base::Time::FromMillisecondsSinceUnixEpoch(time) -
                         ntp_navigation_start_time_);
  }
  Fetch(log_url,
        base::BindOnce(&NewTabPageHandler::OnLogFetchResult,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void NewTabPageHandler::OnDoodleShared(
    new_tab_page::mojom::DoodleShareChannel channel,
    const std::string& doodle_id,
    const std::optional<std::string>& share_id) {
  int channel_id;
  switch (channel) {
    case new_tab_page::mojom::DoodleShareChannel::kFacebook:
      channel_id = 2;
      break;
    case new_tab_page::mojom::DoodleShareChannel::kTwitter:
      channel_id = 3;
      break;
    case new_tab_page::mojom::DoodleShareChannel::kEmail:
      channel_id = 5;
      break;
    case new_tab_page::mojom::DoodleShareChannel::kLinkCopy:
      channel_id = 6;
      break;
    default:
      NOTREACHED();
  }
  std::string query =
      base::StringPrintf("gen_204?atype=i&ct=doodle&ntp=2&cad=sh,%d,ct:%s",
                         channel_id, doodle_id.c_str());
  if (share_id.has_value()) {
    query += "&ei=" + *share_id;
  }
  auto url = GURL(TemplateURLServiceFactory::GetForProfile(profile_)
                      ->search_terms_data()
                      .GoogleBaseURLValue())
                 .Resolve(query);
  // We just ping the server to indicate a doodle has been shared.
  Fetch(url, base::NullCallback());
}

void NewTabPageHandler::OnPromoLinkClicked() {
  LogEvent(NTP_MIDDLE_SLOT_PROMO_LINK_CLICKED);
}

void NewTabPageHandler::IncrementComposeButtonShownCount() {
  const int shown_count = profile_->GetPrefs()->GetInteger(
      prefs::kNtpComposeButtonShownCountPrefName);
  profile_->GetPrefs()->SetInteger(prefs::kNtpComposeButtonShownCountPrefName,
                                   shown_count + 1);
}

void NewTabPageHandler::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  OnThemeChanged();
}

void NewTabPageHandler::OnThemeChanged() {
  page_->SetTheme(MakeTheme(web_contents_->GetColorProvider(), theme_provider_,
                            theme_service_, ntp_custom_background_service_,
                            web_contents_));
}

void NewTabPageHandler::OnCustomBackgroundImageUpdated() {
  OnThemeChanged();
}

void NewTabPageHandler::OnPromoDataUpdated() {
  if (promo_load_start_time_.has_value()) {
    base::TimeDelta duration = base::TimeTicks::Now() - *promo_load_start_time_;
    DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.Promos.RequestLatency2",
                                          duration);
    if (promo_service_->promo_status() == PromoService::Status::OK_WITH_PROMO) {
      DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
          "NewTabPage.Promos.RequestLatency2.SuccessWithPromo", duration);
    } else if (promo_service_->promo_status() ==
               PromoService::Status::OK_BUT_BLOCKED) {
      DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
          "NewTabPage.Promos.RequestLatency2.SuccessButBlocked", duration);
    } else if (promo_service_->promo_status() ==
               PromoService::Status::OK_WITHOUT_PROMO) {
      DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
          "NewTabPage.Promos.RequestLatency2.SuccessWithoutPromo", duration);
    } else {
      DCHECK(promo_service_->promo_status() !=
             PromoService::Status::NOT_UPDATED);
      DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
          "NewTabPage.Promos.RequestLatency2.Failure", duration);
    }
    promo_load_start_time_ = std::nullopt;
  }

  const auto& data = promo_service_->promo_data();
  if (data.has_value() &&
      promo_service_->promo_status() != PromoService::Status::OK_BUT_BLOCKED) {
    page_->SetPromo(MakePromo(data.value()));
  } else {
    page_->SetPromo(nullptr);
  }
}

void NewTabPageHandler::OnPromoServiceShuttingDown() {
  promo_service_observation_.Reset();
  promo_service_ = nullptr;
}

void NewTabPageHandler::OnChangeInFeatureCurrentlyEnabledState(
    bool is_now_enabled) {
  page_->SetWallpaperSearchButtonVisibility(is_now_enabled);
}

void NewTabPageHandler::OnAuthStateUpdated() {
  UpdateModulesLoadable();
}

void NewTabPageHandler::OnFooterVisibilityUpdated(bool visible) {
  page_->FooterVisibilityUpdated(visible);
}

void NewTabPageHandler::OnLogoAvailable(
    GetDoodleCallback callback,
    search_provider_logos::LogoCallbackReason type,
    const std::optional<search_provider_logos::EncodedLogo>& logo) {
  if (!logo) {
    std::move(callback).Run(nullptr);
    return;
  }
  auto doodle = new_tab_page::mojom::Doodle::New();
  if (logo->metadata.type == search_provider_logos::LogoType::SIMPLE ||
      logo->metadata.type == search_provider_logos::LogoType::ANIMATED) {
    if (!logo->encoded_image) {
      std::move(callback).Run(nullptr);
      return;
    }
    auto image_doodle = new_tab_page::mojom::AllModeImageDoodle::New();
    image_doodle->light = MakeImageDoodle(
        logo->metadata.type, logo->encoded_image->as_string(),
        logo->metadata.mime_type, logo->metadata.animated_url,
        logo->metadata.width_px, logo->metadata.height_px, "#ffffff",
        logo->metadata.log_url, logo->metadata.cta_log_url);
    if (logo->dark_encoded_image) {
      image_doodle->dark = MakeImageDoodle(
          logo->metadata.type, logo->dark_encoded_image->as_string(),
          logo->metadata.dark_mime_type, logo->metadata.dark_animated_url,
          logo->metadata.dark_width_px, logo->metadata.dark_height_px,
          logo->metadata.dark_background_color, logo->metadata.dark_log_url,
          logo->metadata.dark_cta_log_url);
    }
    if (logo->metadata.on_click_url.is_valid()) {
      image_doodle->on_click_url = logo->metadata.on_click_url;
    }
    image_doodle->share_url = logo->metadata.short_link;
    doodle->image = std::move(image_doodle);
  } else if (logo->metadata.type ==
             search_provider_logos::LogoType::INTERACTIVE) {
    auto interactive_doodle = new_tab_page::mojom::InteractiveDoodle::New();
    interactive_doodle->url = logo->metadata.full_page_url;
    interactive_doodle->width = logo->metadata.iframe_width_px;
    interactive_doodle->height = logo->metadata.iframe_height_px;
    doodle->interactive = std::move(interactive_doodle);
  } else {
    std::move(callback).Run(nullptr);
    return;
  }
  doodle->description = logo->metadata.alt_text;
  std::move(callback).Run(std::move(doodle));
}

void NewTabPageHandler::OnBrowserWindowInterfaceChanged() {
  if (!base::FeatureList::IsEnabled(ntp_features::kNtpFooter)) {
    return;
  }

  // TODO(webium): FooterController depends on BrowserView, but WebUIBrowser
  // doesn't have a BrowserView.
  if (webui_browser::IsWebUIBrowserEnabled()) {
    return;
  }

  footer_controller_observation_.Reset();
  auto* browser = webui::GetBrowserWindowInterface(web_contents_);
  if (!browser) {
    // TODO(crbug.com/378475391): NTP should always load into a WebContents
    // owned by a TabModel. Remove this once NTP loading has been restricted to
    // browser tabs only.
    return;
  }

  auto* footer_controller = browser->GetFeatures().new_tab_footer_controller();
  CHECK(footer_controller);
  footer_controller_observation_.Observe(footer_controller);
}

void NewTabPageHandler::MaybeTriggerAutomaticCustomizeChromePromo() {
  feature_promo_helper_->MaybeTriggerAutomaticCustomizeChromePromo(
      web_contents_);
}

void NewTabPageHandler::LogEvent(NTPLoggingEventType event) {
  logger_.LogEvent(event, base::TimeDelta() /* unused */);
}

void NewTabPageHandler::Fetch(const GURL& url,
                              OnFetchResultCallback on_result) {
  auto traffic_annotation =
      net::DefineNetworkTrafficAnnotation("new_tab_page_handler", R"(
        semantics {
          sender: "New Tab Page"
          description: "Logs impression and interaction with doodle or promo."
          trigger:
            "Showing or clicking on the doodle or promo on the New Tab Page. "
            "Desktop only."
          data:
            "String identifiying todays doodle or promo and token identifying "
            "a single interaction session. Data does not contain PII."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature via selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine'."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
          }
        })");
  auto url_loader_factory = profile_->GetURLLoaderFactory();
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  auto loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  loader->DownloadToString(url_loader_factory.get(),
                           base::BindOnce(&NewTabPageHandler::OnFetchResult,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          loader.get(), std::move(on_result)),
                           kMaxDownloadBytes);
  loader_map_.insert({loader.get(), std::move(loader)});
}

void NewTabPageHandler::OnFetchResult(const network::SimpleURLLoader* loader,
                                      OnFetchResultCallback on_result,
                                      std::optional<std::string> body) {
  bool success = loader->NetError() == net::OK && loader->ResponseInfo() &&
                 loader->ResponseInfo()->headers &&
                 loader->ResponseInfo()->headers->response_code() >= 200 &&
                 loader->ResponseInfo()->headers->response_code() <= 299 &&
                 body;
  if (on_result) {
    std::move(on_result).Run(success, std::move(body));
  }
  loader_map_.erase(loader);
}

void NewTabPageHandler::OnLogFetchResult(OnDoodleImageRenderedCallback callback,
                                         bool success,
                                         std::optional<std::string> body) {
  CHECK(!success || body);
  if (!success || body->size() < 4 || body->substr(0, 4) != ")]}'") {
    std::move(callback).Run("", std::nullopt, "");
    return;
  }
  auto value = base::JSONReader::Read(body->substr(4),
                                      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!value.has_value()) {
    std::move(callback).Run("", std::nullopt, "");
    return;
  }

  base::Value::Dict& dict = value->GetDict();
  auto* target_url_params_value =
      dict.FindStringByDottedPath("ddllog.target_url_params");
  auto target_url_params =
      target_url_params_value ? *target_url_params_value : "";
  auto* interaction_log_url_value =
      dict.FindStringByDottedPath("ddllog.interaction_log_url");
  auto interaction_log_url =
      interaction_log_url_value
          ? std::optional<GURL>(
                GURL(TemplateURLServiceFactory::GetForProfile(profile_)
                         ->search_terms_data()
                         .GoogleBaseURLValue())
                    .Resolve(*interaction_log_url_value))
          : std::nullopt;
  auto* encoded_ei_value = dict.FindStringByDottedPath("ddllog.encoded_ei");
  auto encoded_ei = encoded_ei_value ? *encoded_ei_value : "";
  std::move(callback).Run(target_url_params, interaction_log_url, encoded_ei);
}

ntp_tiles::TileType NewTabPageHandler::GetTileType() const {
  // TODO(crbug.com/444707872): Update logic to account for multi-select
  // support for shortcuts.
  if (profile_->GetPrefs()->GetBoolean(
          ntp_prefs::kNtpEnterpriseShortcutsVisible)) {
    return ntp_tiles::TileType::kEnterpriseShortcuts;
  }
  return profile_->GetPrefs()->GetBoolean(ntp_prefs::kNtpCustomLinksVisible)
             ? ntp_tiles::TileType::kCustomLinks
             : ntp_tiles::TileType::kTopSites;
}

bool NewTabPageHandler::IsActionChipsVisible() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kNtpToolChipsVisible);
}

bool NewTabPageHandler::IsShortcutsVisible() const {
  return profile_->GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible);
}

void NewTabPageHandler::MaybeLaunchInteractionSurvey(
    std::string_view interaction,
    const std::string& module_id,
    int delay_time_ms) {
  const auto& module_trigger_id =
      GetSurveyTriggerIdForModuleAndInteraction(interaction, module_id);
  if (module_trigger_id.empty()) {
    return;
  }

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);
  CHECK(hats_service);
  hats_service->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerNtpModules, web_contents_, delay_time_ms, {}, {},
      HatsService::NavigationBehavior::ALLOW_ANY, base::DoNothing(),
      base::DoNothing(), module_trigger_id);
}

void NewTabPageHandler::MaybeShowWebstoreToast() {
  if (profile_->GetPrefs()->GetInteger(prefs::kSeedColorChangeCount) <= 3) {
    page_->ShowWebstoreToast();
  }
}

void NewTabPageHandler::RecordModuleInteraction(const std::string& module_id) {
  DisableModuleAutoRemoval(profile_, module_id);
  IncrementDictPrefKeyCount(prefs::kNtpModulesInteractedCountDict, module_id);
}

void NewTabPageHandler::IncrementDictPrefKeyCount(const std::string& pref_name,
                                                  const std::string& key) {
  const base::Value::Dict& counts_dict =
      profile_->GetPrefs()->GetDict(pref_name);
  std::optional<int> count = counts_dict.FindInt(key);
  ScopedDictPrefUpdate update(profile_->GetPrefs(), pref_name);
  update->Set(key,
              count.has_value()
                  ? ((count.value() < INT_MAX) ? count.value() + 1 : INT_MAX)
                  : 1);
}

const std::string& NewTabPageHandler::GetSurveyTriggerIdForModuleAndInteraction(
    std::string_view interaction,
    const std::string& module_id) {
  static const std::string kNoTriggerId;
  DCHECK(kModuleInteractionNames.find(interaction) !=
         kModuleInteractionNames.end());
  const base::Value::Dict* module_id_trigger_dict =
      interaction_module_id_trigger_dict_.FindDict(interaction);
  if (module_id_trigger_dict) {
    auto* trigger_id = module_id_trigger_dict->FindString(module_id);
    if (trigger_id) {
      return *trigger_id;
    }
  }

  return kNoTriggerId;
}

void NewTabPageHandler::SetModuleHidden(const std::string& module_id,
                                        bool hidden) {
  ScopedListPrefUpdate update(profile_->GetPrefs(), prefs::kNtpHiddenModules);
  base::Value::List& list = update.Get();
  base::Value module_id_value(module_id);
  if (hidden) {
    if (!base::Contains(list, module_id_value)) {
      list.Append(std::move(module_id_value));
    }
  } else {
    list.EraseValue(module_id_value);
  }
}

bool NewTabPageHandler::SyncMicrosoftModulesWithAuth() {
  MicrosoftAuthService::AuthState state =
      microsoft_auth_service_->GetAuthState();

  const std::vector<std::string> auth_dependent_modules(
      ntp_modules::kMicrosoftAuthDependentModuleIds.begin(),
      ntp_modules::kMicrosoftAuthDependentModuleIds.end());
  const std::string auth_id = ntp_modules::kMicrosoftAuthenticationModuleId;
  std::vector<std::string> enabled_modules;
  std::vector<std::string> disabled_modules;
  switch (state) {
    case MicrosoftAuthService::AuthState::kNone:
      break;
    case MicrosoftAuthService::AuthState::kError:
      enabled_modules.push_back(auth_id);
      disabled_modules = auth_dependent_modules;
      break;
    case MicrosoftAuthService::AuthState::kSuccess:
      enabled_modules = auth_dependent_modules;
      disabled_modules.push_back(auth_id);
      break;
    default:
      NOTREACHED();
  }

  for (const auto& module_id : enabled_modules) {
    SetModuleHidden(module_id, false);
  }
  for (const auto& module_id : disabled_modules) {
    SetModuleHidden(module_id, true);
  }

  return state != MicrosoftAuthService::AuthState::kNone;
}

void NewTabPageHandler::ConnectToParentDocument(
    mojo::PendingRemote<new_tab_page::mojom::MicrosoftAuthUntrustedDocument>
        child_untrusted_document_remote) {
  page_->ConnectToParentDocument(std::move(child_untrusted_document_remote));
}
