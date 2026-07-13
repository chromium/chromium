// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/ai_mode_page_action_controller.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/to_address.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ai_mode_button_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/page_action/page_action_observer.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/ai_mode_button_config.h"
#include "components/search_engines/ai_mode_button_service.h"
#include "components/search_engines/search_engine_type.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "skia/ext/image_operations.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace {

const char kIconSourceHistogram[] = "Omnibox.AiModePageAction.IconSource";

page_actions::PageActionController* GetPageActionController(
    BrowserWindowInterface& bwi) {
  tabs::TabInterface* tab_interface = bwi.GetActiveTabInterface();
  if (!tab_interface || !tab_interface->GetTabFeatures()) {
    return nullptr;
  }

  return tab_interface->GetTabFeatures()->page_action_controller();
}

// Converts an `Image` of arbitrary size to an `ImageModel` of
// `kLocationBarChipIconSize` size.
ui::ImageModel SizedImageModel(const gfx::Image& image) {
  gfx::Size target_size = {
      GetLayoutConstant(LayoutConstant::kLocationBarChipIconSize),
      GetLayoutConstant(LayoutConstant::kLocationBarChipIconSize)};
  if (image.Size() == target_size) {
    return ui::ImageModel::FromImage(image);
  }

  gfx::ImageSkia image_skia = image.AsImageSkia();
  image_skia = gfx::ImageSkiaOperations::CreateResizedImage(
      image_skia, skia::ImageOperations::RESIZE_BEST, target_size);
  return ui::ImageModel::FromImageSkia(image_skia);
}

}  // namespace

namespace omnibox {

DEFINE_USER_DATA(AiModePageActionController);

AiModePageActionController::AiModePageActionController(
    BrowserWindowInterface& bwi,
    Profile& profile,
    LocationBar& location_bar)
    : page_actions::PageActionObserver(kActionAiMode),
      bwi_(bwi),
      profile_(profile),
      location_bar_(location_bar),
      scoped_data_(bwi.GetUnownedUserDataHost(), *this) {
  CHECK(IsPageActionMigrated(PageActionIconType::kAiMode));

  if (auto* omnibox_controller = location_bar.GetOmniboxController()) {
    omnibox_edit_model_observation_.Observe(omnibox_controller->edit_model());
  }

  auto* ai_mode_button_service =
      AiModeButtonServiceFactory::GetForProfile(&profile);
  CHECK(ai_mode_button_service);
  ai_mode_config_subscription_ =
      ai_mode_button_service->RegisterOnConfigChanged(
          base::IgnoreArgs<const ai_mode_button_config::AiModeButtonConfig*>(
              base::BindRepeating(&AiModePageActionController::UpdatePageAction,
                                  weak_factory_.GetWeakPtr())));
}

AiModePageActionController::~AiModePageActionController() = default;

void AiModePageActionController::OnContentsChanged() {
  UpdatePageAction();
}

void AiModePageActionController::UpdatePageAction() {
  page_actions::PageActionController* page_action_controller =
      GetPageActionController(*bwi_);
  if (!page_action_controller) {
    return;
  }

  // Register as observer for first time or update for tab change.
  RegisterAsPageActionObserver(*page_action_controller);

  const bool is_visible =
      ShouldShowPageAction(base::to_address(profile_), *location_bar_);

  if (is_visible) {
    NotifyOmniboxTriggeredFeatureService(
        *location_bar_->GetOmniboxController());
  }
  UpdatePageActionUi(is_visible);
}

// static
AiModePageActionController* AiModePageActionController::From(
    BrowserWindowInterface* bwi) {
  return bwi ? Get(bwi->GetUnownedUserDataHost()) : nullptr;
}

// static
void AiModePageActionController::OpenAiMode(
    OmniboxController& omnibox_controller,
    bool via_keyboard) {
  omnibox_controller.edit_model()->OpenAiMode(
      via_keyboard ? OmniboxEditModel::AimActivation::kKeyboard
                   : OmniboxEditModel::AimActivation::kClickOrGesture);
}

// static
void AiModePageActionController::NotifyOmniboxTriggeredFeatureService(
    const OmniboxController& omnibox_controller) {
  const auto* client = omnibox_controller.autocomplete_controller()
                           ->autocomplete_provider_client();
  auto* triggered_feature_service = client->GetOmniboxTriggeredFeatureService();
  triggered_feature_service->FeatureTriggered(
      ::metrics::OmniboxEventProto_Feature::
          OmniboxEventProto_Feature_AIM_PAGE_ACTION_OMNIBOX_ENTRYPOINT);
}

// static
bool AiModePageActionController::ShouldShowPageAction(
    Profile* profile,
    LocationBar& location_bar) {
  if (!profile->GetPrefs()->GetBoolean(omnibox::kShowAiModeOmniboxButton)) {
    return false;
  }

  const auto* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  auto* ai_mode_button_service =
      AiModeButtonServiceFactory::GetForProfile(profile);
  const auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (!OmniboxFieldTrial::IsAimOmniboxEntrypointEnabled(aim_eligibility_service,
                                                        ai_mode_button_service,
                                                        template_url_service)) {
    return false;
  }

  auto* omnibox_controller = location_bar.GetOmniboxController();
  if (!omnibox_controller) {
    return false;
  }

  const OmniboxEditModel* edit_model = omnibox_controller->edit_model();

  // If the user is currently in keyword mode, then suppress the AIM entrypoint.
  if (edit_model->is_keyword_selected()) {
    return false;
  }

  // If the feature is enabled to hide the AIM entrypoint on user input, don't
  // show the AIM entrypoint if the user typed text is non-empty.
  if (base::FeatureList::IsEnabled(omnibox::kHideAimEntrypointOnUserInput) &&
      !edit_model->user_text().empty()) {
    return false;
  }

  if (omnibox::kShowRhsAimHint.Get()) {
    return false;
  }

  // If the feature is enabled to hide the AIM entrypoint for URL suggestions,
  // don't show the AIM entrypoint if the default match is a URL suggestion.
  if (base::FeatureList::IsEnabled(
          omnibox::kHideAimEntrypointForUrlSuggestions) ||
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxDynamicAiModeButton)) {
    const AutocompleteResult& result =
        omnibox_controller->autocomplete_controller()->result();
    if (result.default_match() &&
        !AutocompleteMatch::IsSearchType(result.default_match()->type)) {
      return false;
    }
  }

  // Otherwise, we should show the AIM view if the focus is within any view in
  // the location bar, including the omnibox, this view or any other page action
  // icon views.
  const bool has_focus = location_bar.IsFocusWithin();

  const auto page_classification = edit_model->GetPageClassification();

  // Suppress the AI Mode page action button when OnFocus on web pages.
  if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxDynamicAiModeButton)) {
    if (has_focus && !edit_model->user_input_in_progress() &&
        !omnibox::IsNTPPage(page_classification)) {
      return false;
    }
  }

  // TODO(crbug.com/448234135): Remove this logic from the migrated path when
  // Page Action framework supports suggestion chip queueing.
  //
  // Handle the edge case in non-NTP page context with omnibox focus and closed
  // popup. In this case, we suppress the AIM page action in order to ensure
  // that it doesn't get visually "sandwiched" in between the other page actions
  // that show up in this state.
  if (has_focus && !edit_model->user_input_in_progress() &&
      !location_bar.GetOmniboxController()->IsPopupOpen() &&
      !omnibox::IsNTPPage(page_classification)) {
    return false;
  }

  return has_focus;
}

void AiModePageActionController::OnPageActionIconShown(
    const page_actions::PageActionState& page_action) {
  DCHECK_EQ(page_action.action_id, kActionAiMode);
  is_visible_ = true;
}

void AiModePageActionController::OnPageActionIconHidden(
    const page_actions::PageActionState& page_action) {
  DCHECK_EQ(page_action.action_id, kActionAiMode);
  is_visible_ = false;
}

void AiModePageActionController::UpdatePageActionUi(bool is_visible) {
  if (!is_visible) {
    favicon_fetch_weak_factory_.InvalidateWeakPtrs();
    Hide(IconSource::kInvisible);
    return;
  }

  page_actions::PageActionController* page_action_controller =
      GetPageActionController(*bwi_);
  CHECK(page_action_controller);
  auto* service =
      AiModeButtonServiceFactory::GetForProfile(base::to_address(profile_));
  CHECK(service);
  auto* config = service->GetCurrentConfig();
  CHECK(config);

  page_action_controller->OverrideText(kActionAiMode, config->text);
  page_action_controller->OverrideTooltip(kActionAiMode, config->tooltip);
  page_action_controller->OverrideAccessibleName(kActionAiMode,
                                                 config->a11y_label);

  if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxDynamicAiModeButton)) {
    page_action_controller->SetAnimationStyle(
        kActionAiMode,
        page_actions::PageActionAnimationStyle::kSlideAndCrossfade);
    page_action_controller->SetTrailingImage(
        kActionAiMode,
        ui::ImageModel::FromVectorIcon(vector_icons::kArrowForwardIcon));

    bool has_user_input = false;
    if (auto* omnibox_controller = location_bar_->GetOmniboxController()) {
      has_user_input = omnibox_controller->edit_model() &&
                       !omnibox_controller->edit_model()->user_text().empty();
    }
    page_action_controller->SetShowTrailingIcon(kActionAiMode, has_user_input);
  }

  ImageCacheKey key{config->id, GURL(config->favicon_url).spec()};

  if (base::FeatureList::IsEnabled(features::kAiModePageActionOptimization) &&
      cached_image_model_.has_value() && cached_image_key_ == key) {
    ShowAndOverrideImage(*cached_image_model_, key,
                         config->id == SearchEngineType::SEARCH_ENGINE_GOOGLE
                             ? IconSource::kVectorIcon
                             : IconSource::kMemoryFaviconCache);
    return;
  }

  if (config->id == SearchEngineType::SEARCH_ENGINE_GOOGLE) {
    ui::ImageModel image_model = ui::ImageModel::FromImageGenerator(
        base::BindRepeating([](const ui::ColorProvider* color_provider) {
          return gfx::CreateVectorIcon(
              features::IsRoundedIconsEnabled() ? omnibox::kSearchSparkIcon
                                                : omnibox::kSearchSparkOldIcon,
              GetLayoutConstant(LayoutConstant::kLocationBarChipIconSize),
              color_provider->GetColor(kColorOmniboxIconForegroundTonal));
        }),
        gfx::Size(GetLayoutConstant(LayoutConstant::kLocationBarChipIconSize),
                  GetLayoutConstant(LayoutConstant::kLocationBarChipIconSize)));
    ShowAndOverrideImage(image_model, key, IconSource::kVectorIcon);

  } else {
    GURL favicon_url(config->favicon_url);
    OmniboxClient* client = location_bar_->GetOmniboxController()->client();
    gfx::Image image = client->GetFaviconForIconUrl(
        favicon_url,
        base::BindOnce(&AiModePageActionController::OnFaviconFetchedLocally,
                       favicon_fetch_weak_factory_.GetWeakPtr(), favicon_url),
        /*notify_on_empty=*/true);
    // `image` will be empty if not cached. In which case, let
    // `OnFaviconFetchedLocally()` handle visibility and the image.
    if (!image.IsEmpty()) {
      ShowAndOverrideImage(SizedImageModel(image), key,
                           IconSource::kMemoryFaviconCache);
    }
  }
}

void AiModePageActionController::Hide(IconSource source) {
  base::UmaHistogramEnumeration(kIconSourceHistogram, source);
  if (page_actions::PageActionController* page_action_controller =
          GetPageActionController(*bwi_)) {
    page_action_controller->HideSuggestionChip(kActionAiMode);
    page_action_controller->Hide(kActionAiMode);
  }
}

void AiModePageActionController::ShowAndOverrideImage(
    const ui::ImageModel& image_model,
    const ImageCacheKey& key,
    IconSource source) {
  base::UmaHistogramEnumeration(kIconSourceHistogram, source);
  if (base::FeatureList::IsEnabled(features::kAiModePageActionOptimization)) {
    cached_image_model_ = image_model;
    cached_image_key_ = key;
  }
  if (page_actions::PageActionController* page_action_controller =
          GetPageActionController(*bwi_)) {
    page_action_controller->OverrideImage(kActionAiMode, image_model);
    page_action_controller->Show(kActionAiMode);
    page_action_controller->ShowSuggestionChip(kActionAiMode,
                                               {.should_animate = false});
  }
}

void AiModePageActionController::OnFaviconFetchedLocally(
    const GURL& favicon_url,
    const gfx::Image& favicon) {
  // If visibility became false, this callback should have been cancelled.
  CHECK(ShouldShowPageAction(base::to_address(profile_), *location_bar_));

  // If the config changed, this callback should have been cancelled.
  auto* service =
      AiModeButtonServiceFactory::GetForProfile(base::to_address(profile_));
  CHECK(service);
  auto* config = service->GetCurrentConfig();
  CHECK(config);
  CHECK_EQ(GURL{config->favicon_url}, favicon_url);

  if (favicon.IsEmpty()) {
    FetchFaviconFromNetwork(favicon_url);
    return;
  }
  ShowAndOverrideImage(SizedImageModel(favicon),
                       ImageCacheKey{config->id, favicon_url.spec()},
                       IconSource::kDiskDbFaviconCache);
}

void AiModePageActionController::FetchFaviconFromNetwork(
    const GURL& favicon_url) {
  BitmapFetcherService* fetcher_service =
      BitmapFetcherServiceFactory::GetForBrowserContext(
          base::to_address(profile_));
  if (!fetcher_service) {
    Hide(IconSource::kFailedIcon);
    return;
  }

  fetcher_service->RequestImage(
      favicon_url,
      base::BindOnce(&AiModePageActionController::OnFaviconFetchedFromNetwork,
                     favicon_fetch_weak_factory_.GetWeakPtr(), favicon_url));
}

void AiModePageActionController::OnFaviconFetchedFromNetwork(
    const GURL& favicon_url,
    SkBitmap bitmap) {
  // If visibility became false, this callback should have been cancelled.
  CHECK(ShouldShowPageAction(base::to_address(profile_), *location_bar_));

  // If the config changed, this callback should have been cancelled.
  auto* service =
      AiModeButtonServiceFactory::GetForProfile(base::to_address(profile_));
  CHECK(service);
  auto* config = service->GetCurrentConfig();
  CHECK(config);
  CHECK_EQ(GURL{config->favicon_url}, favicon_url);

  if (bitmap.empty()) {
    Hide(IconSource::kFailedIcon);
    return;
  }

  // Store fetched icon into favicon cache.
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(base::to_address(profile_),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (favicon_service) {
    favicon_service->SetFavicons({favicon_url}, favicon_url,
                                 favicon_base::IconType::kFavicon,
                                 gfx::Image::CreateFrom1xBitmap(bitmap));
  }

  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  ShowAndOverrideImage(SizedImageModel(gfx::Image(image_skia)),
                       ImageCacheKey{config->id, favicon_url.spec()},
                       IconSource::kNetworkFetch);
}

}  // namespace omnibox
