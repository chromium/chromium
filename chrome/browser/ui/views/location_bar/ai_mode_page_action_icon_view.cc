// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/ai_mode_page_action_icon_view.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/grit/branded_strings.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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

  // The accessible name should show the full text, independent of the what the
  // label text is set to.
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_AI_MODE_ENTRYPOINT_LABEL),
      ax::mojom::NameFrom::kAttribute);
}

AiModePageActionIconView::~AiModePageActionIconView() = default;

void AiModePageActionIconView::OnExecuting(
    PageActionIconView::ExecuteSource source) {
  OmniboxView* omnibox_view = GetOmniboxView();
  CHECK(omnibox_view);
  omnibox_view->model()->OpenAiMode();
}

views::BubbleDialogDelegate* AiModePageActionIconView::GetBubble() const {
  return nullptr;
}

const gfx::VectorIcon& AiModePageActionIconView::GetVectorIcon() const {
  return omnibox::kSearchSparkIcon;
}

void AiModePageActionIconView::ExecuteWithKeyboardSourceForTesting() {
  CHECK(GetVisible());
  OnExecuting(EXECUTE_SOURCE_KEYBOARD);
}

void AiModePageActionIconView::UpdateImpl() {
  SetVisible(ShouldShow());
  ResetSlideAnimation(true);
}

bool AiModePageActionIconView::ShouldShow() {
  // Show the AIM view if the focus is within any view in the location bar,
  // including the omnibox, this view or any other page action icon views.
  View* location_bar_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kLocationBarElementId,
          views::ElementTrackerViews::GetContextForView(this));
  if (!location_bar_view) {
    return false;
  }
  const views::FocusManager* const focus_manager = GetFocusManager();
  return focus_manager &&
         location_bar_view->Contains(focus_manager->GetFocusedView());
}

OmniboxView* AiModePageActionIconView::GetOmniboxView() {
  auto* web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }
  return search::GetOmniboxView(web_contents);
}

BEGIN_METADATA(AiModePageActionIconView)
END_METADATA
