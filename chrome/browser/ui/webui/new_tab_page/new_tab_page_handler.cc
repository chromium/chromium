// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"

#include <algorithm>

#include "base/base64.h"
#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_background_service.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service_factory.h"
#include "chrome/browser/search/promos/promo_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_provider_logos/logo_service_factory.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/webui/realbox/realbox.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_provider_logos/logo_service.h"
#include "components/search_provider_logos/switches.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"

namespace {

const int64_t kMaxDownloadBytes = 1024 * 1024;

new_tab_page::mojom::ThemePtr MakeTheme(const NtpTheme& ntp_theme) {
  auto theme = new_tab_page::mojom::Theme::New();
  theme->is_default = ntp_theme.using_default_theme;
  theme->background_color = ntp_theme.background_color;
  theme->shortcut_background_color = ntp_theme.shortcut_color;
  theme->shortcut_text_color = ntp_theme.text_color;
  theme->shortcut_use_white_add_icon =
      color_utils::IsDark(ntp_theme.shortcut_color);
  theme->shortcut_use_title_pill = false;
  theme->is_dark = !color_utils::IsDark(ntp_theme.text_color);
  if (ntp_theme.logo_alternate) {
    theme->logo_color = ntp_theme.logo_color;
  }
  auto background_image = new_tab_page::mojom::BackgroundImage::New();
  if (!ntp_theme.custom_background_url.is_empty()) {
    background_image->url = ntp_theme.custom_background_url;
  } else if (ntp_theme.has_theme_image) {
    theme->shortcut_use_title_pill = true;
    background_image->url =
        GURL(base::StrCat({"chrome-untrusted://theme/IDR_THEME_NTP_BACKGROUND?",
                           ntp_theme.theme_id}));
    background_image->url_2x = GURL(
        base::StrCat({"chrome-untrusted://theme/IDR_THEME_NTP_BACKGROUND@2x?",
                      ntp_theme.theme_id}));
    if (ntp_theme.has_attribution) {
      background_image->attribution_url = GURL(base::StrCat(
          {"chrome://theme/IDR_THEME_NTP_ATTRIBUTION?", ntp_theme.theme_id}));
    }
    background_image->size = "initial";
    switch (ntp_theme.image_tiling) {
      case THEME_BKGRND_IMAGE_NO_REPEAT:
        background_image->repeat_x = "no-repeat";
        background_image->repeat_y = "no-repeat";
        break;
      case THEME_BKGRND_IMAGE_REPEAT_X:
        background_image->repeat_x = "repeat";
        background_image->repeat_y = "no-repeat";
        break;
      case THEME_BKGRND_IMAGE_REPEAT_Y:
        background_image->repeat_x = "no-repeat";
        background_image->repeat_y = "repeat";
        break;
      case THEME_BKGRND_IMAGE_REPEAT:
        background_image->repeat_x = "repeat";
        background_image->repeat_y = "repeat";
        break;
    }
    switch (ntp_theme.image_horizontal_alignment) {
      case THEME_BKGRND_IMAGE_ALIGN_CENTER:
        background_image->position_x = "center";
        break;
      case THEME_BKGRND_IMAGE_ALIGN_LEFT:
        background_image->position_x = "left";
        break;
      case THEME_BKGRND_IMAGE_ALIGN_RIGHT:
        background_image->position_x = "right";
        break;
      case THEME_BKGRND_IMAGE_ALIGN_TOP:
      case THEME_BKGRND_IMAGE_ALIGN_BOTTOM:
        // Inconsistent. Ignore.
        break;
    }
    switch (ntp_theme.image_vertical_alignment) {
      case THEME_BKGRND_IMAGE_ALIGN_CENTER:
        background_image->position_y = "center";
        break;
      case THEME_BKGRND_IMAGE_ALIGN_TOP:
        background_image->position_y = "top";
        break;
      case THEME_BKGRND_IMAGE_ALIGN_BOTTOM:
        background_image->position_y = "bottom";
        break;
      case THEME_BKGRND_IMAGE_ALIGN_LEFT:
      case THEME_BKGRND_IMAGE_ALIGN_RIGHT:
        // Inconsistent. Ignore.
        break;
    }
  } else {
    background_image = nullptr;
  }
  theme->background_image = std::move(background_image);
  if (!ntp_theme.custom_background_attribution_line_1.empty()) {
    theme->background_image_attribution_1 =
        ntp_theme.custom_background_attribution_line_1;
  }
  if (!ntp_theme.custom_background_attribution_line_2.empty()) {
    theme->background_image_attribution_2 =
        ntp_theme.custom_background_attribution_line_2;
  }
  if (!ntp_theme.custom_background_attribution_action_url.is_empty()) {
    theme->background_image_attribution_url =
        ntp_theme.custom_background_attribution_action_url;
  }
  if (!ntp_theme.collection_id.empty()) {
    theme->daily_refresh_collection_id = ntp_theme.collection_id;
  }

  auto search_box = realbox::mojom::SearchBoxTheme::New();
  search_box->bg = ntp_theme.search_box.bg;
  search_box->icon = ntp_theme.search_box.icon;
  search_box->icon_selected = ntp_theme.search_box.icon_selected;
  search_box->placeholder = ntp_theme.search_box.placeholder;
  search_box->results_bg = ntp_theme.search_box.results_bg;
  search_box->results_bg_hovered = ntp_theme.search_box.results_bg_hovered;
  search_box->results_bg_selected = ntp_theme.search_box.results_bg_selected;
  search_box->results_dim = ntp_theme.search_box.results_dim;
  search_box->results_dim_selected = ntp_theme.search_box.results_dim_selected;
  search_box->results_text = ntp_theme.search_box.results_text;
  search_box->results_text_selected =
      ntp_theme.search_box.results_text_selected;
  search_box->results_url = ntp_theme.search_box.results_url;
  search_box->results_url_selected = ntp_theme.search_box.results_url_selected;
  search_box->text = ntp_theme.search_box.text;
  theme->search_box = std::move(search_box);

  return theme;
}

ntp_tiles::NTPTileImpression MakeNTPTileImpression(
    const new_tab_page::mojom::MostVisitedTile& tile,
    uint32_t index) {
  return ntp_tiles::NTPTileImpression(
      /*index=*/index,
      /*source=*/static_cast<ntp_tiles::TileSource>(tile.source),
      /*title_source=*/
      static_cast<ntp_tiles::TileTitleSource>(tile.title_source),
      /*visual_type=*/
      ntp_tiles::TileVisualType::ICON_REAL /* unused on desktop */,
      /*icon_type=*/favicon_base::IconType::kInvalid /* unused on desktop */,
      /*data_generation_time=*/tile.data_generation_time,
      /*url_for_rappor=*/GURL() /* unused */);
}

SkColor ParseHexColor(const std::string& color) {
  SkColor result;
  if (color.size() == 7 && color[0] == '#' &&
      base::HexStringToUInt(color.substr(1), &result)) {
    return SkColorSetA(result, 255);
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
    int share_button_x,
    int share_button_y,
    const std::string& share_button_icon,
    const std::string& share_button_bg,
    double share_button_opacity,
    GURL log_url,
    GURL cta_log_url) {
  auto doodle = new_tab_page::mojom::ImageDoodle::New();
  std::string base64;
  base::Base64Encode(data, &base64);
  doodle->image_url = GURL(base::StringPrintf(
      "data:%s;base64,%s", mime_type.c_str(), base64.c_str()));
  if (type == search_provider_logos::LogoType::ANIMATED) {
    doodle->animation_url = animated_url;
  }
  doodle->width = width_px;
  doodle->height = height_px;
  doodle->background_color = ParseHexColor(background_color);
  if (!share_button_icon.empty()) {
    doodle->share_button = new_tab_page::mojom::DoodleShareButton::New();
    doodle->share_button->x = share_button_x;
    doodle->share_button->y = share_button_y;
    doodle->share_button->icon_url = GURL(base::StringPrintf(
        "data:image/png;base64,%s", share_button_icon.c_str()));
    doodle->share_button->background_color =
        SkColorSetA(ParseHexColor(share_button_bg),
                    std::max(0.0, std::min(share_button_opacity, 1.0)) * 255.0);
  }
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
  auto middle_slot = base::JSONReader::Read(data.middle_slot_json);
  if (!middle_slot.has_value() ||
      middle_slot.value().FindBoolPath("hidden").value_or(false)) {
    return nullptr;
  }
  auto promo = new_tab_page::mojom::Promo::New();
  promo->id = data.promo_id;
  if (middle_slot.has_value()) {
    auto* parts = middle_slot.value().FindListPath("part");
    if (parts) {
      std::vector<new_tab_page::mojom::PromoPartPtr> mojom_parts;
      for (const base::Value& part : parts->GetList()) {
        if (part.FindKey("image")) {
          auto mojom_image = new_tab_page::mojom::PromoImagePart::New();
          auto* image_url = part.FindStringPath("image.image_url");
          if (!image_url || image_url->empty()) {
            continue;
          }
          mojom_image->image_url = GURL(*image_url);
          auto* target = part.FindStringPath("image.target");
          if (target && !target->empty()) {
            mojom_image->target = GURL(*target);
          }
          mojom_parts.push_back(
              new_tab_page::mojom::PromoPart::NewImage(std::move(mojom_image)));
        } else if (part.FindKey("link")) {
          auto mojom_link = new_tab_page::mojom::PromoLinkPart::New();
          auto* url = part.FindStringPath("link.url");
          if (!url || url->empty()) {
            continue;
          }
          mojom_link->url = GURL(*url);
          auto* text = part.FindStringPath("link.text");
          if (!text || text->empty()) {
            continue;
          }
          mojom_link->text = *text;
          auto* color = part.FindStringPath("link.color");
          if (color && !color->empty()) {
            mojom_link->color = *color;
          }
          mojom_parts.push_back(
              new_tab_page::mojom::PromoPart::NewLink(std::move(mojom_link)));
        } else if (part.FindKey("text")) {
          auto mojom_text = new_tab_page::mojom::PromoTextPart::New();
          auto* text = part.FindStringPath("text.text");
          if (!text || text->empty()) {
            continue;
          }
          mojom_text->text = *text;
          auto* color = part.FindStringPath("text.color");
          if (color && !color->empty()) {
            mojom_text->color = *color;
          }
          mojom_parts.push_back(
              new_tab_page::mojom::PromoPart::NewText(std::move(mojom_text)));
        }
      }
      promo->middle_slot_parts = std::move(mojom_parts);
    }
  }
  promo->log_url = data.promo_log_url;
  return promo;
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
    InstantService* instant_service,
    content::WebContents* web_contents,
    const base::Time& ntp_navigation_start_time)
    : instant_service_(instant_service),
      ntp_background_service_(
          NtpBackgroundServiceFactory::GetForProfile(profile)),
      logo_service_(LogoServiceFactory::GetForProfile(profile)),
      one_google_bar_service_(
          OneGoogleBarServiceFactory::GetForProfile(profile)),
      profile_(profile),
      web_contents_(web_contents),
      ntp_navigation_start_time_(ntp_navigation_start_time),
      logger_(profile, GURL(chrome::kChromeUINewTabPageURL)),
      promo_service_(PromoServiceFactory::GetForProfile(profile)),
      page_{std::move(pending_page)},
      receiver_{this, std::move(pending_page_handler)} {
  CHECK(instant_service_);
  CHECK(ntp_background_service_);
  CHECK(logo_service_);
  CHECK(one_google_bar_service_);
  CHECK(promo_service_);
  CHECK(web_contents_);
  instant_service_->AddObserver(this);
  ntp_background_service_->AddObserver(this);
  instant_service_->UpdateNtpTheme();
  promo_service_observation_.Observe(promo_service_);
  one_google_bar_service_observation_.Observe(one_google_bar_service_);
}

NewTabPageHandler::~NewTabPageHandler() {
  instant_service_->RemoveObserver(this);
  ntp_background_service_->RemoveObserver(this);
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

// static
void NewTabPageHandler::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kNtpModulesVisible, true);
  registry->RegisterListPref(prefs::kNtpDisabledModules, true);
}

void NewTabPageHandler::AddMostVisitedTile(
    const GURL& url,
    const std::string& title,
    AddMostVisitedTileCallback callback) {
  bool success = instant_service_->AddCustomLink(url, title);
  std::move(callback).Run(success);
  logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_ADD, base::TimeDelta() /* unused */);
}

void NewTabPageHandler::DeleteMostVisitedTile(const GURL& url) {
  if (instant_service_->IsCustomLinksEnabled()) {
    instant_service_->DeleteCustomLink(url);
    logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_REMOVE,
                     base::TimeDelta() /* unused */);
  } else {
    instant_service_->DeleteMostVisitedItem(url);
    last_blocklisted_ = url;
  }
}

void NewTabPageHandler::RestoreMostVisitedDefaults() {
  if (instant_service_->IsCustomLinksEnabled()) {
    instant_service_->ResetCustomLinks();
    logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_RESTORE_ALL,
                     base::TimeDelta() /* unused */);
  } else {
    instant_service_->UndoAllMostVisitedDeletions();
  }
}

void NewTabPageHandler::ReorderMostVisitedTile(const GURL& url,
                                               uint8_t new_pos) {
  instant_service_->ReorderCustomLink(url, new_pos);
}

void NewTabPageHandler::SetMostVisitedSettings(bool custom_links_enabled,
                                               bool visible) {
  auto pair = instant_service_->GetCurrentShortcutSettings();
  // The first of the pair is true if most-visited tiles are being used.
  bool old_custom_links_enabled = !pair.first;
  bool old_visible = pair.second;
  // |ToggleMostVisitedOrCustomLinks()| always notifies observers. Since we only
  // want to notify once, we need to call |ToggleShortcutsVisibility()| with
  // false if we are also going to call |ToggleMostVisitedOrCustomLinks()|.
  bool toggleCustomLinksEnabled =
      old_custom_links_enabled != custom_links_enabled;
  if (old_visible != visible) {
    instant_service_->ToggleShortcutsVisibility(
        /* do_notify= */ !toggleCustomLinksEnabled);
    logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_TOGGLE_VISIBILITY,
                     base::TimeDelta() /* unused */);
  }
  if (toggleCustomLinksEnabled) {
    instant_service_->ToggleMostVisitedOrCustomLinks();
    logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_TOGGLE_TYPE,
                     base::TimeDelta() /* unused */);
  }
}

void NewTabPageHandler::UndoMostVisitedTileAction() {
  if (instant_service_->IsCustomLinksEnabled()) {
    instant_service_->UndoCustomLinkAction();
    logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_UNDO,
                     base::TimeDelta() /* unused */);
  } else if (last_blocklisted_.is_valid()) {
    instant_service_->UndoMostVisitedDeletion(last_blocklisted_);
    last_blocklisted_ = GURL();
  }
}

void NewTabPageHandler::SetBackgroundImage(const std::string& attribution_1,
                                           const std::string& attribution_2,
                                           const GURL& attribution_url,
                                           const GURL& image_url) {
  // Populating the |collection_id| turns on refresh daily which overrides the
  // the selected image.
  instant_service_->SetCustomBackgroundInfo(image_url, attribution_1,
                                            attribution_2, attribution_url,
                                            /* collection_id= */ "");
  LogEvent(NTP_BACKGROUND_IMAGE_SET);
}

void NewTabPageHandler::SetDailyRefreshCollectionId(
    const std::string& collection_id) {
  // Populating the |collection_id| turns on refresh daily which overrides the
  // the selected image.
  instant_service_->SetCustomBackgroundInfo(
      /* image_url */ GURL(), /* attribution_1= */ "", /* attribution_2= */ "",
      /* attribution_url= */ GURL(), collection_id);
  LogEvent(NTP_BACKGROUND_DAILY_REFRESH_ENABLED);
}

void NewTabPageHandler::SetNoBackgroundImage() {
  instant_service_->SetCustomBackgroundInfo(
      /* image_url */ GURL(), /* attribution_1= */ "", /* attribution_2= */ "",
      /* attribution_url= */ GURL(), /* collection_id= */ "");
  LogEvent(NTP_BACKGROUND_IMAGE_RESET);
}

void NewTabPageHandler::UpdateMostVisitedInfo() {
  // OnNewTabPageOpened refreshes the most visited entries while
  // UpdateMostVisitedInfo triggers a call to MostVisitedInfoChanged.
  instant_service_->OnNewTabPageOpened();
  instant_service_->UpdateMostVisitedInfo();
}

void NewTabPageHandler::UpdateMostVisitedTile(
    const GURL& url,
    const GURL& new_url,
    const std::string& new_title,
    UpdateMostVisitedTileCallback callback) {
  bool success = instant_service_->UpdateCustomLink(
      url, new_url != url ? new_url : GURL(), new_title);
  std::move(callback).Run(success);
  logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_UPDATE,
                   base::TimeDelta() /* unused */);
}

void NewTabPageHandler::GetBackgroundCollections(
    GetBackgroundCollectionsCallback callback) {
  if (!ntp_background_service_ || background_collections_callback_) {
    std::move(callback).Run(
        std::vector<new_tab_page::mojom::BackgroundCollectionPtr>());
    return;
  }
  background_collections_request_start_time_ = base::TimeTicks::Now();
  background_collections_callback_ = std::move(callback);
  ntp_background_service_->FetchCollectionInfo();
}

void NewTabPageHandler::GetBackgroundImages(
    const std::string& collection_id,
    GetBackgroundImagesCallback callback) {
  if (background_images_callback_) {
    std::move(background_images_callback_)
        .Run(std::vector<new_tab_page::mojom::CollectionImagePtr>());
  }
  if (!ntp_background_service_) {
    std::move(callback).Run(
        std::vector<new_tab_page::mojom::CollectionImagePtr>());
    return;
  }
  images_request_collection_id_ = collection_id;
  background_images_request_start_time_ = base::TimeTicks::Now();
  background_images_callback_ = std::move(callback);
  ntp_background_service_->FetchCollectionImageInfo(collection_id);
}

void NewTabPageHandler::GetDoodle(GetDoodleCallback callback) {
  search_provider_logos::LogoCallbacks callbacks;
  std::string fresh_doodle_param;
  if (net::GetValueForKeyInQuery(web_contents_->GetLastCommittedURL(),
                                 "fresh-doodle", &fresh_doodle_param) &&
      fresh_doodle_param == "1") {
    // In fresh-doodle mode, wait for the desired doodle to be downloaded.
    callbacks.on_fresh_encoded_logo_available =
        base::BindOnce(&NewTabPageHandler::OnLogoAvailable,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  } else {
    // In regular mode, return cached doodle as it is available faster.
    callbacks.on_cached_encoded_logo_available =
        base::BindOnce(&NewTabPageHandler::OnLogoAvailable,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  }
  // This will trigger re-downloading the doodle and caching it. This means that
  // in regular mode a new doodle will be returned on subsequent NTP loads.
  logo_service_->GetLogo(std::move(callbacks), /*for_webui_ntp=*/true);
}

void NewTabPageHandler::ChooseLocalCustomBackground(
    ChooseLocalCustomBackgroundCallback callback) {
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents_));
  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.allowed_paths = ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH;
  file_types.extensions.resize(1);
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("jpg"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("jpeg"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("png"));
  file_types.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_UPLOAD_IMAGE_FORMAT));
  choose_local_custom_background_callback_ = std::move(callback);
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, std::u16string(),
      profile_->last_selected_directory(), &file_types, 0,
      base::FilePath::StringType(), web_contents_->GetTopLevelNativeWindow(),
      nullptr);
}

void NewTabPageHandler::GetOneGoogleBarParts(
    const std::string& query_params,
    GetOneGoogleBarPartsCallback callback) {
  if (!one_google_bar_service_) {
    return;
  }
  one_google_bar_parts_callbacks_.push_back(std::move(callback));
  bool wait_for_refresh =
      one_google_bar_service_->SetAdditionalQueryParams(query_params);
  if (one_google_bar_service_->one_google_bar_data().has_value() &&
      !wait_for_refresh &&
      base::FeatureList::IsEnabled(ntp_features::kCacheOneGoogleBar)) {
    OnOneGoogleBarDataUpdated();
  }
  one_google_bar_load_start_time_ = base::TimeTicks::Now();
  one_google_bar_service_->Refresh();
}

void NewTabPageHandler::GetPromo(GetPromoCallback callback) {
  // Replace the promo URL with "command:<id>" if such a command ID is set
  // via the feature params.
  const std::string command_id = base::GetFieldTrialParamValueByFeature(
      features::kPromoBrowserCommands, features::kPromoBrowserCommandIdParam);
  if (!command_id.empty()) {
    auto promo = new_tab_page::mojom::Promo::New();
    std::vector<new_tab_page::mojom::PromoPartPtr> parts;
    auto image = new_tab_page::mojom::PromoImagePart::New();
    // Warning symbol used as the test image.
    image->image_url = GURL(
        "data:image/"
        "svg+xml;base64,"
        "PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9Ii01IC"
        "01IDU4IDU4IiBmaWxsPSIjZmRkNjMzIj48cGF0aCBkPSJNMiA0Mmg0NEwyNCA0IDIgNDJ6"
        "bTI0LTZoLTR2LTRoNHY0em0wLThoLTR2LThoNHY4eiIvPjwvc3ZnPg==");
    image->target = GURL("command:" + command_id);
    parts.push_back(new_tab_page::mojom::PromoPart::NewImage(std::move(image)));
    auto link = new_tab_page::mojom::PromoLinkPart::New();
    link->url = GURL("command:" + command_id);
    link->text = "Test command: " + command_id;
    parts.push_back(new_tab_page::mojom::PromoPart::NewLink(std::move(link)));
    promo->middle_slot_parts = std::move(parts);
    std::move(callback).Run(std::move(promo));
    return;
  }

  promo_callbacks_.push_back(std::move(callback));
  if (promo_service_->promo_data().has_value()) {
    OnPromoDataUpdated();
  }
  promo_load_start_time_ = base::TimeTicks::Now();
  promo_service_->Refresh();
}

void NewTabPageHandler::OnDismissModule(const std::string& module_id) {
  const std::string histogram_prefix(kModuleDismissedHistogram);
  base::UmaHistogramExactLinear(histogram_prefix, 1, 1);
  base::UmaHistogramExactLinear(histogram_prefix + "." + module_id, 1, 1);
}

void NewTabPageHandler::OnRestoreModule(const std::string& module_id) {
  const std::string histogram_prefix(kModuleRestoredHistogram);
  base::UmaHistogramExactLinear(histogram_prefix, 1, 1);
  base::UmaHistogramExactLinear(histogram_prefix + "." + module_id, 1, 1);
}

void NewTabPageHandler::SetModulesVisible(bool visible) {
  profile_->GetPrefs()->SetBoolean(prefs::kNtpModulesVisible, visible);
  UpdateDisabledModules();
}

void NewTabPageHandler::SetModuleDisabled(const std::string& module_id,
                                          bool disabled) {
  ListPrefUpdate update(profile_->GetPrefs(), prefs::kNtpDisabledModules);
  if (disabled) {
    update->AppendIfNotPresent(std::make_unique<base::Value>(module_id));
  } else {
    update->EraseListValue(base::Value(module_id));
  }
  UpdateDisabledModules();
}

void NewTabPageHandler::UpdateDisabledModules() {
  std::vector<std::string> module_ids;
  // If the module visibility is managed by policy we either disable all modules
  // (if invisible) or no modules (if visible).
  if (!profile_->GetPrefs()->IsManagedPreference(prefs::kNtpModulesVisible)) {
    const auto* module_ids_value =
        profile_->GetPrefs()->GetList(prefs::kNtpDisabledModules);
    for (const auto& id : *module_ids_value) {
      module_ids.push_back(id.GetString());
    }
  }
  page_->SetDisabledModules(
      !profile_->GetPrefs()->GetBoolean(prefs::kNtpModulesVisible),
      std::move(module_ids));
}

void NewTabPageHandler::OnModulesLoadedWithData() {
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);
  CHECK(hats_service);
  hats_service->LaunchDelayedSurveyForWebContents(kHatsSurveyTriggerNtpModules,
                                                  web_contents_, 0);
}

void NewTabPageHandler::OnPromoDataUpdated() {
  if (promo_load_start_time_.has_value()) {
    base::TimeDelta duration = base::TimeTicks::Now() - *promo_load_start_time_;
    UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.Promos.RequestLatency2", duration);
    if (promo_service_->promo_status() == PromoService::Status::OK_WITH_PROMO) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "NewTabPage.Promos.RequestLatency2.SuccessWithPromo", duration);
    } else if (promo_service_->promo_status() ==
               PromoService::Status::OK_BUT_BLOCKED) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "NewTabPage.Promos.RequestLatency2.SuccessButBlocked", duration);
    } else if (promo_service_->promo_status() ==
               PromoService::Status::OK_WITHOUT_PROMO) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "NewTabPage.Promos.RequestLatency2.SuccessWithoutPromo", duration);
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.Promos.RequestLatency2.Failure",
                                 duration);
    }
    promo_load_start_time_ = base::nullopt;
  }

  const auto& data = promo_service_->promo_data();
  for (auto& callback : promo_callbacks_) {
    if (data.has_value() && !data->promo_html.empty()) {
      std::move(callback).Run(MakePromo(data.value()));
    } else {
      std::move(callback).Run(nullptr);
    }
  }
  promo_callbacks_.clear();
}

void NewTabPageHandler::OnPromoServiceShuttingDown() {
  promo_service_observation_.Reset();
  promo_service_ = nullptr;
}

void NewTabPageHandler::OnAppRendered(double time) {
  logger_.LogEvent(NTP_APP_RENDERED,
                   base::Time::FromJsTime(time) - ntp_navigation_start_time_);
}

void NewTabPageHandler::OnMostVisitedTilesRendered(
    std::vector<new_tab_page::mojom::MostVisitedTilePtr> tiles,
    double time) {
  for (size_t i = 0; i < tiles.size(); i++) {
    logger_.LogMostVisitedImpression(MakeNTPTileImpression(*tiles[i], i));
  }
  // This call flushes all most visited impression logs to UMA histograms.
  // Therefore, it must come last.
  logger_.LogEvent(NTP_ALL_TILES_LOADED,
                   base::Time::FromJsTime(time) - ntp_navigation_start_time_);
}

void NewTabPageHandler::OnOneGoogleBarRendered(double time) {
  logger_.LogEvent(NTP_ONE_GOOGLE_BAR_SHOWN,
                   base::Time::FromJsTime(time) - ntp_navigation_start_time_);
}

void NewTabPageHandler::OnPromoRendered(double time,
                                        const base::Optional<GURL>& log_url) {
  logger_.LogEvent(NTP_MIDDLE_SLOT_PROMO_SHOWN,
                   base::Time::FromJsTime(time) - ntp_navigation_start_time_);
  if (log_url.has_value() && log_url->is_valid()) {
    Fetch(*log_url, base::BindOnce([](bool, std::unique_ptr<std::string>) {}));
  }
}

void NewTabPageHandler::OnMostVisitedTileNavigation(
    new_tab_page::mojom::MostVisitedTilePtr tile,
    uint32_t index,
    uint8_t mouse_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key) {
  logger_.LogMostVisitedNavigation(MakeNTPTileImpression(*tile, index));

  if (!base::FeatureList::IsEnabled(
          ntp_features::kNtpHandleMostVisitedNavigationExplicitly))
    return;

  WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);
  // Clicks on the MV tiles should be treated as if the user clicked on a
  // bookmark. This is consistent with Android's native implementation and
  // ensures the visit count for the MV entry is updated.
  // Use a link transition for query tiles, e.g., repeatable queries, so that
  // their visit count is not updated by this navigation. Otherwise duplicate
  // query tiles could also be offered as most visited.
  // |is_query_tile| can be true only when ntp_features::kNtpRepeatableQueries
  // is enabled.
  web_contents_->OpenURL(content::OpenURLParams(
      tile->url, content::Referrer(), disposition,
      tile->is_query_tile ? ui::PAGE_TRANSITION_LINK
                          : ui::PAGE_TRANSITION_AUTO_BOOKMARK,
      false));
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
    const base::Optional<::GURL>& log_url) {
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
      return;
  }
  LogEvent(event);

  if (type == new_tab_page::mojom::DoodleImageType::kCta &&
      log_url.has_value()) {
    // We just ping the server to indicate a CTA image has been clicked.
    Fetch(*log_url, base::BindOnce([](bool, std::unique_ptr<std::string>) {}));
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
                     base::Time::FromJsTime(time) - ntp_navigation_start_time_);
  }
  Fetch(log_url,
        base::BindOnce(&NewTabPageHandler::OnLogFetchResult,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void NewTabPageHandler::OnDoodleShared(
    new_tab_page::mojom::DoodleShareChannel channel,
    const std::string& doodle_id,
    const base::Optional<std::string>& share_id) {
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
      break;
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
  Fetch(url, base::BindOnce([](bool s, std::unique_ptr<std::string>) {}));
}

void NewTabPageHandler::OnPromoLinkClicked() {
  LogEvent(NTP_MIDDLE_SLOT_PROMO_LINK_CLICKED);
}

void NewTabPageHandler::OnVoiceSearchAction(
    new_tab_page::mojom::VoiceSearchAction action) {
  NTPLoggingEventType event;
  switch (action) {
    case new_tab_page::mojom::VoiceSearchAction::kActivateSearchBox:
      event = NTP_VOICE_ACTION_ACTIVATE_SEARCH_BOX;
      break;
    case new_tab_page::mojom::VoiceSearchAction::kActivateKeyboard:
      event = NTP_VOICE_ACTION_ACTIVATE_KEYBOARD;
      break;
    case new_tab_page::mojom::VoiceSearchAction::kCloseOverlay:
      event = NTP_VOICE_ACTION_CLOSE_OVERLAY;
      break;
    case new_tab_page::mojom::VoiceSearchAction::kQuerySubmitted:
      event = NTP_VOICE_ACTION_QUERY_SUBMITTED;
      break;
    case new_tab_page::mojom::VoiceSearchAction::kSupportLinkClicked:
      event = NTP_VOICE_ACTION_SUPPORT_LINK_CLICKED;
      break;
    case new_tab_page::mojom::VoiceSearchAction::kTryAgainLink:
      event = NTP_VOICE_ACTION_TRY_AGAIN_LINK;
      break;
    case new_tab_page::mojom::VoiceSearchAction::kTryAgainMicButton:
      event = NTP_VOICE_ACTION_TRY_AGAIN_MIC_BUTTON;
      break;
  }
  LogEvent(event);
}

void NewTabPageHandler::OnVoiceSearchError(
    new_tab_page::mojom::VoiceSearchError error) {
  NTPLoggingEventType event;
  switch (error) {
    case new_tab_page::mojom::VoiceSearchError::kAborted:
      event = NTP_VOICE_ERROR_ABORTED;
      break;
    case new_tab_page::mojom::VoiceSearchError::kNoSpeech:
      event = NTP_VOICE_ERROR_NO_SPEECH;
      break;
    case new_tab_page::mojom::VoiceSearchError::kAudioCapture:
      event = NTP_VOICE_ERROR_AUDIO_CAPTURE;
      break;
    case new_tab_page::mojom::VoiceSearchError::kNetwork:
      event = NTP_VOICE_ERROR_NETWORK;
      break;
    case new_tab_page::mojom::VoiceSearchError::kNotAllowed:
      event = NTP_VOICE_ERROR_NOT_ALLOWED;
      break;
    case new_tab_page::mojom::VoiceSearchError::kLanguageNotSupported:
      event = NTP_VOICE_ERROR_LANGUAGE_NOT_SUPPORTED;
      break;
    case new_tab_page::mojom::VoiceSearchError::kNoMatch:
      event = NTP_VOICE_ERROR_NO_MATCH;
      break;
    case new_tab_page::mojom::VoiceSearchError::kServiceNotAllowed:
      event = NTP_VOICE_ERROR_SERVICE_NOT_ALLOWED;
      break;
    case new_tab_page::mojom::VoiceSearchError::kBadGrammar:
      event = NTP_VOICE_ERROR_BAD_GRAMMAR;
      break;
    case new_tab_page::mojom::VoiceSearchError::kOther:
      event = NTP_VOICE_ERROR_OTHER;
      break;
  }
  LogEvent(event);
}

void NewTabPageHandler::NtpThemeChanged(const NtpTheme& ntp_theme) {
  page_->SetTheme(MakeTheme(ntp_theme));
}

void NewTabPageHandler::MostVisitedInfoChanged(
    const InstantMostVisitedInfo& info) {
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  std::vector<new_tab_page::mojom::MostVisitedTilePtr> list;
  auto result = new_tab_page::mojom::MostVisitedInfo::New();
  for (auto& tile : info.items) {
    auto value = new_tab_page::mojom::MostVisitedTile::New();
    if (tile.title.empty()) {
      value->title = tile.url.spec();
      value->title_direction = base::i18n::LEFT_TO_RIGHT;
    } else {
      value->title = base::UTF16ToUTF8(tile.title);
      value->title_direction =
          base::i18n::GetFirstStrongCharacterDirection(tile.title);
    }
    value->url = tile.url;
    value->source = static_cast<int32_t>(tile.source);
    value->title_source = static_cast<int32_t>(tile.title_source);
    value->data_generation_time = tile.data_generation_time;
    value->is_query_tile =
        base::FeatureList::IsEnabled(ntp_features::kNtpRepeatableQueries) &&
        template_url_service &&
        template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
            tile.url);
    list.push_back(std::move(value));
  }
  result->custom_links_enabled = !info.use_most_visited;
  result->tiles = std::move(list);
  result->visible = info.is_visible;
  page_->SetMostVisitedInfo(std::move(result));
}

void NewTabPageHandler::OnCollectionInfoAvailable() {
  if (!background_collections_callback_) {
    return;
  }

  base::TimeDelta duration =
      base::TimeTicks::Now() - background_collections_request_start_time_;
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "NewTabPage.BackgroundService.Collections.RequestLatency", duration);
  // Any response where no collections are returned is considered a failure.
  if (ntp_background_service_->collection_info().empty()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Collections.RequestLatency.Failure",
        duration);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Collections.RequestLatency.Success",
        duration);
  }

  std::vector<new_tab_page::mojom::BackgroundCollectionPtr> collections;
  for (const auto& info : ntp_background_service_->collection_info()) {
    auto collection = new_tab_page::mojom::BackgroundCollection::New();
    collection->id = info.collection_id;
    collection->label = info.collection_name;
    collection->preview_image_url = GURL(info.preview_image_url);
    collections.push_back(std::move(collection));
  }
  std::move(background_collections_callback_).Run(std::move(collections));
}

void NewTabPageHandler::OnCollectionImagesAvailable() {
  if (!background_images_callback_) {
    return;
  }

  base::TimeDelta duration =
      base::TimeTicks::Now() - background_images_request_start_time_;
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "NewTabPage.BackgroundService.Images.RequestLatency", duration);
  // Any response where no images are returned is considered a failure.
  if (ntp_background_service_->collection_images().empty()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Images.RequestLatency.Failure", duration);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Images.RequestLatency.Success", duration);
  }

  std::vector<new_tab_page::mojom::CollectionImagePtr> images;
  if (ntp_background_service_->collection_images().empty()) {
    std::move(background_images_callback_).Run(std::move(images));
    return;
  }
  auto collection_id =
      ntp_background_service_->collection_images()[0].collection_id;
  for (const auto& info : ntp_background_service_->collection_images()) {
    DCHECK(info.collection_id == collection_id);
    auto image = new_tab_page::mojom::CollectionImage::New();
    image->attribution_1 = !info.attribution.empty() ? info.attribution[0] : "";
    image->attribution_2 =
        info.attribution.size() > 1 ? info.attribution[1] : "";
    image->attribution_url = info.attribution_action_url;
    image->image_url = info.image_url;
    image->preview_image_url = info.thumbnail_image_url;
    images.push_back(std::move(image));
  }
  std::move(background_images_callback_).Run(std::move(images));
}

void NewTabPageHandler::OnNextCollectionImageAvailable() {}

void NewTabPageHandler::OnNtpBackgroundServiceShuttingDown() {
  ntp_background_service_->RemoveObserver(this);
  ntp_background_service_ = nullptr;
}

void NewTabPageHandler::OnOneGoogleBarDataUpdated() {
  base::Optional<OneGoogleBarData> data =
      one_google_bar_service_->one_google_bar_data();

  if (one_google_bar_load_start_time_.has_value()) {
    NTPUserDataLogger::LogOneGoogleBarFetchDuration(
        /*success=*/data.has_value(),
        /*duration=*/base::TimeTicks::Now() - *one_google_bar_load_start_time_);
    one_google_bar_load_start_time_ = base::nullopt;
  }

  for (auto& callback : one_google_bar_parts_callbacks_) {
    if (data.has_value()) {
      auto parts = new_tab_page::mojom::OneGoogleBarParts::New();
      parts->bar_html = data->bar_html;
      parts->in_head_script = data->in_head_script;
      parts->in_head_style = data->in_head_style;
      parts->after_bar_script = data->after_bar_script;
      parts->end_of_body_html = data->end_of_body_html;
      parts->end_of_body_script = data->end_of_body_script;
      std::move(callback).Run(std::move(parts));
    } else {
      std::move(callback).Run(nullptr);
    }
  }
  one_google_bar_parts_callbacks_.clear();
}

void NewTabPageHandler::OnOneGoogleBarServiceShuttingDown() {
  one_google_bar_service_observation_.Reset();
  one_google_bar_service_ = nullptr;
}

void NewTabPageHandler::FileSelected(const base::FilePath& path,
                                     int index,
                                     void* params) {
  DCHECK(choose_local_custom_background_callback_);
  if (instant_service_) {
    profile_->set_last_selected_directory(path.DirName());
    instant_service_->SelectLocalBackgroundImage(path);
  }

  select_file_dialog_ = nullptr;
  // File selection can happen at any time after NTP load, and is not logged
  // with the event.
  LogEvent(NTP_CUSTOMIZE_LOCAL_IMAGE_DONE);
  LogEvent(NTP_BACKGROUND_UPLOAD_DONE);

  if (choose_local_custom_background_callback_)
    std::move(choose_local_custom_background_callback_).Run(true);
}

void NewTabPageHandler::FileSelectionCanceled(void* params) {
  DCHECK(choose_local_custom_background_callback_);
  select_file_dialog_ = nullptr;
  // File selection can happen at any time after NTP load, and is not logged
  // with the event.
  LogEvent(NTP_CUSTOMIZE_LOCAL_IMAGE_CANCEL);
  LogEvent(NTP_BACKGROUND_UPLOAD_CANCEL);
  if (choose_local_custom_background_callback_)
    std::move(choose_local_custom_background_callback_).Run(false);
}

void NewTabPageHandler::OnLogoAvailable(
    GetDoodleCallback callback,
    search_provider_logos::LogoCallbackReason type,
    const base::Optional<search_provider_logos::EncodedLogo>& logo) {
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
        logo->metadata.type, logo->encoded_image->data(),
        logo->metadata.mime_type, logo->metadata.animated_url,
        logo->metadata.width_px, logo->metadata.height_px, "#ffffff",
        logo->metadata.share_button_x, logo->metadata.share_button_y,
        logo->metadata.share_button_icon, logo->metadata.share_button_bg,
        logo->metadata.share_button_opacity, logo->metadata.log_url,
        logo->metadata.cta_log_url);
    if (logo->dark_encoded_image) {
      image_doodle->dark = MakeImageDoodle(
          logo->metadata.type, logo->dark_encoded_image->data(),
          logo->metadata.dark_mime_type, logo->metadata.dark_animated_url,
          logo->metadata.dark_width_px, logo->metadata.dark_height_px,
          logo->metadata.dark_background_color,
          logo->metadata.dark_share_button_x,
          logo->metadata.dark_share_button_y,
          logo->metadata.dark_share_button_icon,
          logo->metadata.dark_share_button_bg,
          logo->metadata.dark_share_button_opacity, logo->metadata.dark_log_url,
          logo->metadata.dark_cta_log_url);
    }
    image_doodle->on_click_url = logo->metadata.on_click_url;
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
  auto url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess();
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
                                      std::unique_ptr<std::string> body) {
  bool success = loader->NetError() == net::OK && loader->ResponseInfo() &&
                 loader->ResponseInfo()->headers &&
                 loader->ResponseInfo()->headers->response_code() >= 200 &&
                 loader->ResponseInfo()->headers->response_code() <= 299 &&
                 body;
  std::move(on_result).Run(success, std::move(body));
  loader_map_.erase(loader);
}

void NewTabPageHandler::OnLogFetchResult(OnDoodleImageRenderedCallback callback,
                                         bool success,
                                         std::unique_ptr<std::string> body) {
  if (!success || body->size() < 4 || body->substr(0, 4) != ")]}'") {
    std::move(callback).Run("", base::nullopt, "");
    return;
  }
  auto value = base::JSONReader::Read(body->substr(4));
  if (!value.has_value()) {
    std::move(callback).Run("", base::nullopt, "");
    return;
  }

  auto* target_url_params_value = value->FindPath("ddllog.target_url_params");
  auto target_url_params =
      target_url_params_value && target_url_params_value->is_string()
          ? target_url_params_value->GetString()
          : "";
  auto* interaction_log_url_value =
      value->FindPath("ddllog.interaction_log_url");
  auto interaction_log_url =
      interaction_log_url_value && interaction_log_url_value->is_string()
          ? base::Optional<GURL>(
                GURL(TemplateURLServiceFactory::GetForProfile(profile_)
                         ->search_terms_data()
                         .GoogleBaseURLValue())
                    .Resolve(interaction_log_url_value->GetString()))
          : base::nullopt;
  auto* encoded_ei_value = value->FindPath("ddllog.encoded_ei");
  auto encoded_ei = encoded_ei_value && encoded_ei_value->is_string()
                        ? encoded_ei_value->GetString()
                        : "";
  std::move(callback).Run(target_url_params, interaction_log_url, encoded_ei);
}
