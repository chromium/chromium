// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_icon_view.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"
#include "chrome/browser/ui/views/translate/translate_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"

TranslateIconView::TranslateIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate,
    Browser* browser)
    : PageActionIconView(command_updater,
                         IDC_SHOW_TRANSLATE,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "Translate",
                         kActionShowTranslate),
      browser_(browser) {
  SetID(VIEW_ID_TRANSLATE_BUTTON);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_TRANSLATE));
}

TranslateIconView::~TranslateIconView() = default;

views::BubbleDialogDelegate* TranslateIconView::GetBubble() const {
  if (GetWebContents()) {
    TranslateBubbleController* translate_bubble_controller =
        TranslateBubbleController::FromWebContents(GetWebContents());

    if (translate_bubble_controller) {
      return translate_bubble_controller->GetTranslateBubble();
    }
  }

  return nullptr;
}

views::BubbleDialogDelegate* TranslateIconView::GetPartialTranslateBubble()
    const {
  if (GetWebContents()) {
    TranslateBubbleController* translate_bubble_controller =
        TranslateBubbleController::FromWebContents(GetWebContents());

    if (translate_bubble_controller) {
      return translate_bubble_controller->GetPartialTranslateBubble();
    }
  }

  return nullptr;
}

bool TranslateIconView::IsBubbleShowing() const {
  // We override the PageActionIconView implementation because there are two
  // different bubbles that may be shown with the Translate icon, and so this
  // function should return true if either the Full Page Translate or Partial
  // Translate bubble are showing. If a bubble is being destroyed, it's
  // considered showing though it may be already invisible currently.
  return (GetBubble() != nullptr) || (GetPartialTranslateBubble() != nullptr);
}

void TranslateIconView::UpdateImpl() {
  if (!GetWebContents()) {
    return;
  }

  const translate::LanguageState& language_state =
      ChromeTranslateClient::FromWebContents(GetWebContents())
          ->GetLanguageState();
  bool enabled = language_state.translate_enabled();

  bool show_page_action = true;
  if (features::IsToolbarPinningEnabled()) {
    CHECK(browser_);
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
    CHECK(browser_view);
    auto* pinned_toolbar_actions_container =
        browser_view->toolbar()->pinned_toolbar_actions_container();
    if (pinned_toolbar_actions_container &&
        pinned_toolbar_actions_container->IsActionPinnedOrPoppedOut(
            action_id().value())) {
      show_page_action = false;
    }
  }
  ChromeTranslateClient::FromWebContents(GetWebContents())
      ->GetTranslateManager()
      ->GetActiveTranslateMetricsLogger()
      ->LogOmniboxIconChange(show_page_action && enabled);
  SetVisible(show_page_action && enabled);

  if (!enabled &&
      TranslateBubbleController::FromWebContents(GetWebContents())) {
    TranslateBubbleController::FromWebContents(GetWebContents())->CloseBubble();
  }
}

void TranslateIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& TranslateIconView::GetVectorIcon() const {
  return vector_icons::kTranslateIcon;
}

BEGIN_METADATA(TranslateIconView)
END_METADATA
