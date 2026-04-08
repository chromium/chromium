// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility_annotator/accessibility_annotator_info_dialog_controller.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/accessibility_annotator/accessibility_annotator_info_dialog.h"
#include "chrome/browser/ui/webui/accessibility_annotator/accessibility_annotator_info_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace accessibility_annotator::info {

const char AccessibilityAnnotatorInfoDialogController::kUserDataKey[] =
    "AccessibilityAnnotatorInfoDialogController";

AccessibilityAnnotatorInfoDialogController::
    AccessibilityAnnotatorInfoDialogController(
        content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

AccessibilityAnnotatorInfoDialogController::
    ~AccessibilityAnnotatorInfoDialogController() {
  CloseDialog();
}

void AccessibilityAnnotatorInfoDialogController::ShowDialog(
    content::WebContents* web_contents,
    base::OnceCallback<void(InfoDialogResult)> callback) {
  if (dialog_widget_) {
    // Dialog is already open, focus it or ignore.
    dialog_widget_->Show();
    return;
  }

  auto contents_wrapper =
      std::make_unique<WebUIContentsWrapperT<AccessibilityAnnotatorInfoUI>>(
          GURL("chrome://accessibility-annotator-info/"),
          Profile::FromBrowserContext(browser_context_),
          0, /* task_manager_string_id */
          /*esc_closes_ui=*/true,
          /*supports_draggable_regions=*/false);

  if (AccessibilityAnnotatorInfoUI* info_ui =
          contents_wrapper->GetWebUIController()) {
    // Wrap the callback to close the bubble when executed.
    auto wrapped_callback = base::BindOnce(
        [](base::WeakPtr<AccessibilityAnnotatorInfoDialogController> controller,
           base::OnceCallback<void(InfoDialogResult)> user_callback,
           InfoDialogResult result) {
          if (controller) {
            controller->CloseDialog();
          }
          if (user_callback) {
            std::move(user_callback).Run(result);
          }
        },
        weak_factory_.GetWeakPtr(), std::move(callback));

    info_ui->SetDialogCallback(std::move(wrapped_callback));
  }

  auto dialog_view = std::make_unique<AccessibilityAnnotatorInfoDialog>(
      nullptr, std::move(contents_wrapper));

  bool use_web_modal = false;
  if (web_contents) {
    auto* manager =
        web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
    if (manager && manager->delegate()) {
      use_web_modal = true;
    }
  }

  if (use_web_modal) {
    dialog_view->SetModalType(ui::mojom::ModalType::kChild);
    dialog_view->SetOwnershipOfNewWidget(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    dialog_widget_ = constrained_window::ShowWebModalDialogViewsOwned(
        dialog_view.release(), web_contents,
        views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    // ShowWebModalDialogViews handles showing the widget
  } else {
    dialog_view->set_has_parent(false);
    auto* widget = views::BubbleDialogDelegateView::CreateBubble(
        std::move(dialog_view), views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    dialog_widget_ = base::WrapUnique(widget);
    dialog_widget_->Show();
  }

  // Ensure that the dialog is closed synchronously when the widget is
  // destroyed.
  dialog_widget_->MakeCloseSynchronous(
      base::IgnoreArgs<views::Widget::ClosedReason>(base::BindOnce(
          &AccessibilityAnnotatorInfoDialogController::CloseDialog,
          weak_factory_.GetWeakPtr())));
}

void AccessibilityAnnotatorInfoDialogController::CloseDialog() {
  dialog_widget_.reset();
}

}  // namespace accessibility_annotator::info
