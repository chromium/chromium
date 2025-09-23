// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/ai_mode_page_action_icon_view.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/grit/branded_strings.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

AiModePageActionIconView::AiModePageActionIconView(
    IconLabelBubbleView::Delegate* parent_delegate,
    Delegate* delegate,
    BrowserWindowInterface* browser)
    : PageActionIconView(nullptr,
                         0,
                         parent_delegate,
                         delegate,
                         "AiMode",
                         kActionAiMode),
      browser_(browser) {
  CHECK(browser_);
  image_container_view()->SetFlipCanvasOnPaintForRTLUI(false);

  SetProperty(views::kElementIdentifierKey, kAiModePageActionIconElementId);

  SetLabel(l10n_util::GetStringUTF16(IDS_AI_MODE_ENTRYPOINT_LABEL));
  SetUseTonalColorsWhenExpanded(true);
  SetBackgroundVisibility(BackgroundVisibility::kWithLabel);

  // The accessible name prompts the user to ask Google AI Mode.
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(
          IDS_STARTER_PACK_AI_MODE_ACTION_SUGGESTION_CONTENTS),
      ax::mojom::NameFrom::kAttribute);
}

AiModePageActionIconView::~AiModePageActionIconView() = default;

void AiModePageActionIconView::OnExecuting(
    PageActionIconView::ExecuteSource source) {
  OmniboxView* omnibox_view = GetOmniboxView();
  CHECK(omnibox_view);
  omnibox_view->model()->OpenAiMode(/*via_keyboard=*/false);
}

views::BubbleDialogDelegate* AiModePageActionIconView::GetBubble() const {
  return nullptr;
}

const gfx::VectorIcon& AiModePageActionIconView::GetVectorIcon() const {
  return omnibox::kSearchSparkIcon;
}

// This event handler exists because, on Mac, the <return> key doesn't activate
// buttons in the omnibox or on the toolbelt. However, this page action is
// designed to act like part of the popup when the popup is open and <return>
// activates it in that state. In order to have consistent behavior, this event
// handler ensures that <return> still activates the behavior when the popup
// *isn't* open.
//
// Other platforms don't require this, so it could be guarded by an IS_MAC build
// flag. However, using this same code path on all platforms may help avoid
// platform-specific bugs.
bool AiModePageActionIconView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN) {
    OmniboxView* omnibox_view = GetOmniboxView();
    CHECK(omnibox_view);
    omnibox_view->model()->OpenAiMode(/*via_keyboard=*/true);
    return true;
  }

  return PageActionIconView::OnKeyPressed(event);
}

void AiModePageActionIconView::ExecuteWithKeyboardSourceForTesting() {
  CHECK(GetVisible());
  OnExecuting(EXECUTE_SOURCE_KEYBOARD);
}

void AiModePageActionIconView::UpdateImpl() {
  const bool should_show = ShouldShow();
  UpdateFeatureTriggered(/*page_action_shown=*/should_show);
  SetVisible(should_show);
  ResetSlideAnimation(true);
}

bool AiModePageActionIconView::ShouldShow() {
  const auto* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(browser_->GetProfile());
  if (!OmniboxFieldTrial::IsAimOmniboxEntrypointEnabled(
          aim_eligibility_service)) {
    return false;
  }

  OmniboxView* omnibox_view = GetOmniboxView();
  if (!omnibox_view) {
    return false;
  }

  // If the user is currently in keyword mode, then suppress the AIM entrypoint.
  if (omnibox_view->model()->is_keyword_selected()) {
    return false;
  }

  // If the feature is enabled to hide the AIM entrypoint on user input, don't
  // show the AIM entrypoint if the user typed text is non-empty.
  if (base::FeatureList::IsEnabled(omnibox::kHideAimEntrypointOnUserInput)) {
    if (!omnibox_view->model()->user_text().empty()) {
      return false;
    }
  }

  // Otherwise, we should show the AIM view if the focus is within any view in
  // the location bar, including the omnibox, this view or any other page action
  // icon views.
  View* location_bar_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kLocationBarElementId,
          views::ElementTrackerViews::GetContextForView(this));
  if (!location_bar_view) {
    return false;
  }
  const views::FocusManager* const focus_manager = GetFocusManager();
  const bool has_focus = focus_manager && location_bar_view->Contains(
                                              focus_manager->GetFocusedView());

  // ...unless the user triggers the following edge-case in the Omnibox while in
  // a non-NTP page context. In this case, we suppress the AIM page action in
  // order to ensure that it doesn't get visually "sandwiched" in between the
  // other page actions that show up in this state.
  const auto page_classification =
      omnibox_view->model()->GetPageClassification();
  const bool is_ntp =
      (page_classification ==
       metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS);
  if (has_focus && !omnibox_view->model()->user_input_in_progress() &&
      !omnibox_view->model()->PopupIsOpen() && !is_ntp) {
    return false;
  }

  return has_focus;
}

OmniboxView* AiModePageActionIconView::GetOmniboxView() {
  auto* web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }
  return search::GetOmniboxView(web_contents);
}

void AiModePageActionIconView::UpdateFeatureTriggered(bool page_action_shown) {
  if (!page_action_shown) {
    return;
  }

  OmniboxView* omnibox_view = GetOmniboxView();
  if (!omnibox_view) {
    return;
  }

  const auto* client = omnibox_view->controller()
                           ->autocomplete_controller()
                           ->autocomplete_provider_client();
  auto* triggered_feature_service = client->GetOmniboxTriggeredFeatureService();
  triggered_feature_service->FeatureTriggered(
      metrics::OmniboxEventProto_Feature::
          OmniboxEventProto_Feature_AIM_PAGE_ACTION_OMNIBOX_ENTRYPOINT);
}

BEGIN_METADATA(AiModePageActionIconView)
END_METADATA
