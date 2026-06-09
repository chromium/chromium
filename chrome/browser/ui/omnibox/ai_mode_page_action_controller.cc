// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/ai_mode_page_action_controller.h"

#include "base/check.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/ai_mode_button_config.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/search_engine_type.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace {

page_actions::PageActionController* GetPageActionController(
    BrowserWindowInterface& bwi) {
  tabs::TabInterface* tab_interface = bwi.GetActiveTabInterface();
  if (!tab_interface || !tab_interface->GetTabFeatures()) {
    return nullptr;
  }

  return tab_interface->GetTabFeatures()->page_action_controller();
}

void SetPageActionVisibility(
    page_actions::PageActionController& page_action_controller,
    OmniboxClient* client,
    base::OnceCallback<void(const gfx::Image&)> on_favicon_fetched,
    bool visible) {
  if (!visible) {
    page_action_controller.HideSuggestionChip(kActionAiMode);
    page_action_controller.Hide(kActionAiMode);
    return;
  }

  const auto& config = ai_mode_button_config::GetCurrentAiModeButtonConfig();
  if (config.id == SearchEngineType::SEARCH_ENGINE_GOOGLE) {
    page_action_controller.OverrideImage(
        kActionAiMode,
        ui::ImageModel::FromImageGenerator(
            base::BindRepeating([](const ui::ColorProvider* color_provider) {
              return gfx::CreateVectorIcon(
                  features::IsRoundedIconsEnabled()
                      ? omnibox::kSearchSparkIcon
                      : omnibox::kSearchSparkOldIcon,
                  GetLayoutConstant(LayoutConstant::kLocationBarChipIconSize),
                  color_provider->GetColor(kColorOmniboxIconForegroundTonal));
            }),
            gfx::Size(
                GetLayoutConstant(LayoutConstant::kLocationBarChipIconSize),
                GetLayoutConstant(LayoutConstant::kLocationBarChipIconSize))));

  } else {
    GURL favicon_url(config.favicon_url);
    gfx::Image image = client->GetFaviconForIconUrl(
        favicon_url, std::move(on_favicon_fetched));
    if (image.IsEmpty()) {
      // `image` will be empty if not cached. Hide this page action until it's
      // returned async.
      page_action_controller.HideSuggestionChip(kActionAiMode);
      page_action_controller.Hide(kActionAiMode);
      return;
    }
    page_action_controller.OverrideImage(kActionAiMode,
                                         ui::ImageModel::FromImage(image));
  }

  page_action_controller.Show(kActionAiMode);
  page_action_controller.ShowSuggestionChip(kActionAiMode,
                                            {.should_animate = false});
}

}  // namespace

namespace omnibox {

DEFINE_USER_DATA(AiModePageActionController);

AiModePageActionController::AiModePageActionController(
    BrowserWindowInterface& bwi,
    Profile& profile,
    LocationBarView& location_bar_view)
    : bwi_(bwi),
      profile_(profile),
      location_bar_view_(location_bar_view),
      scoped_data_(bwi.GetUnownedUserDataHost(), *this) {
  CHECK(IsPageActionMigrated(PageActionIconType::kAiMode));

  if (auto* omnibox_controller = location_bar_view.GetOmniboxController()) {
    observation_.Observe(omnibox_controller->edit_model());
  }
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

  const bool is_visible =
      ShouldShowPageAction(base::to_address(profile_), *location_bar_view_);

  if (is_visible) {
    NotifyOmniboxTriggeredFeatureService(
        *location_bar_view_->GetOmniboxController());
  }
  SetPageActionVisibility(
      *page_action_controller,
      location_bar_view_->GetOmniboxController()->client(),
      base::BindOnce(&AiModePageActionController::OnFaviconFetched,
                     weak_factory_.GetWeakPtr()),
      is_visible);
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
  omnibox_controller.edit_model()->OpenAiMode(via_keyboard,
                                              /*via_context_menu=*/false);
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
    LocationBarView& location_bar_view) {
  if (!profile->GetPrefs()->GetBoolean(omnibox::kShowAiModeOmniboxButton)) {
    return false;
  }

  const auto* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  if (!OmniboxFieldTrial::IsAimOmniboxEntrypointEnabled(
          aim_eligibility_service)) {
    return false;
  }

  auto* omnibox_controller = location_bar_view.GetOmniboxController();
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
      base::FeatureList::IsEnabled(
          omnibox::kWebUIOmniboxDynamicAiModeButton)) {
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
  const views::FocusManager* const focus_manager =
      location_bar_view.GetFocusManager();
  const bool has_focus = focus_manager && location_bar_view.Contains(
                                              focus_manager->GetFocusedView());

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
      !location_bar_view.GetOmniboxController()->IsPopupOpen() &&
      !omnibox::IsNTPPage(page_classification)) {
    return false;
  }

  return has_focus;
}

void AiModePageActionController::OnFaviconFetched(const gfx::Image& favicon) {
  if (favicon.IsEmpty() ||
      !ShouldShowPageAction(base::to_address(profile_), *location_bar_view_)) {
    return;
  }
  if (auto* page_action_controller = GetPageActionController(*bwi_)) {
    page_action_controller->OverrideImage(kActionAiMode,
                                          ui::ImageModel::FromImage(favicon));
    page_action_controller->Show(kActionAiMode);
    page_action_controller->ShowSuggestionChip(kActionAiMode,
                                               {.should_animate = false});
  }
}

}  // namespace omnibox
