// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility_annotator/accessibility_annotator_info_dialog_controller.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/accessibility_annotator/accessibility_annotator_info_dialog.h"
#include "chrome/browser/ui/webui/accessibility_annotator/accessibility_annotator_info_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/constrained_window/constrained_window_views.h"
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
    if (callback) {
      std::move(callback).Run(InfoDialogResult::kDismissed);
    }
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
  } else if (callback) {
    std::move(callback).Run(InfoDialogResult::kDismissed);
  }

  auto dialog_view = std::make_unique<AccessibilityAnnotatorInfoDialog>(
      nullptr, std::move(contents_wrapper));

  dialog_view->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  if (web_contents) {
    dialog_view->SetModalType(ui::mojom::ModalType::kNone);
    dialog_view->set_parent_window(platform_util::GetViewForWindow(
        web_contents->GetTopLevelNativeWindow()));
    dialog_view->SetAnchorRect(web_contents->GetContainerBounds());
  } else {
    dialog_view->set_has_parent(false);
  }

  dialog_widget_ =
      views::BubbleDialogDelegate::CreateBubble(std::move(dialog_view));

  // Ensure that the dialog is closed synchronously when the widget is
  // destroyed.
  dialog_widget_->MakeCloseSynchronous(
      base::IgnoreArgs<views::Widget::ClosedReason>(base::BindOnce(
          &AccessibilityAnnotatorInfoDialogController::CloseDialog,
          weak_factory_.GetWeakPtr())));
}

void AccessibilityAnnotatorInfoDialogController::CloseDialog() {
  if (dialog_widget_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, dialog_widget_.release());
  }
}

}  // namespace accessibility_annotator::info
