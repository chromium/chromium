// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/print_job_confirmation_dialog_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"

// static
void PrintJobConfirmationDialogView::Show(
    gfx::NativeWindow parent,
    const std::string& extension_id,
    const std::u16string& extension_name,
    const gfx::ImageSkia& extension_icon,
    const std::u16string& print_job_title,
    const std::u16string& printer_name,
    base::OnceCallback<void(bool)> callback) {
  // TODO (crbug.com/996785): Extract common code with
  // ExtensionUninstallDialogViews::Show() to separate methods: first method to
  // get an anchor view and the second one to show a BubbleDialogDelegateView.

  // We may want to show dialog even if there is no appropriate browser view,
  // i.e. |parent| is null or kNullNativeWindow. In that case we use
  // constrained_window::CreateBrowserModalDialogViews() (see below).
  BrowserView* const browser_view =
      parent ? BrowserView::GetBrowserViewForNativeWindow(parent) : nullptr;
  ExtensionsToolbarContainer* const container =
      browser_view ? browser_view->toolbar_button_provider()
                         ->GetExtensionsToolbarContainer()
                   : nullptr;
  ToolbarActionView* anchor_view =
      container ? container->GetViewForId(extension_id) : nullptr;

  auto* print_job_confirmation_dialog_view = new PrintJobConfirmationDialogView(
      anchor_view, extension_name, extension_icon, print_job_title,
      printer_name, std::move(callback));
  if (anchor_view) {
    DCHECK(container);
    views::Widget* const widget = views::BubbleDialogDelegateView::CreateBubble(
        print_job_confirmation_dialog_view);
    container->ShowWidgetForExtension(widget, extension_id);
  } else {
    constrained_window::CreateBrowserModalDialogViews(
        print_job_confirmation_dialog_view, parent)
        ->Show();
  }
}

PrintJobConfirmationDialogView::PrintJobConfirmationDialogView(
    ToolbarActionView* anchor_view,
    const std::u16string& extension_name,
    const gfx::ImageSkia& extension_icon,
    const std::u16string& print_job_title,
    const std::u16string& printer_name,
    base::OnceCallback<void(bool)> callback)
    : BubbleDialogDelegateView(anchor_view,
                               anchor_view ? views::BubbleBorder::TOP_RIGHT
                                           : views::BubbleBorder::NONE),
      callback_(std::move(callback)) {
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(
                     IDS_EXTENSIONS_PRINTING_API_PRINT_REQUEST_ALLOW));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(
                     IDS_EXTENSIONS_PRINTING_API_PRINT_REQUEST_DENY));
  SetShowCloseButton(false);
  SetShowIcon(true);
  SetIcon(gfx::ImageSkiaOperations::CreateResizedImage(
      extension_icon, skia::ImageOperations::ResizeMethod::RESIZE_GOOD,
      gfx::Size(extension_misc::EXTENSION_ICON_SMALL,
                extension_misc::EXTENSION_ICON_SMALL)));
  SetTitle(l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_PRINTING_API_PRINT_REQUEST_BUBBLE_TITLE));

  auto run_callback = [](PrintJobConfirmationDialogView* dialog, bool accept) {
    std::move(dialog->callback_).Run(accept);
  };
  SetAcceptCallback(base::BindOnce(run_callback, base::Unretained(this), true));
  SetCancelCallback(
      base::BindOnce(run_callback, base::Unretained(this), false));

  ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  const bool dialog_is_bubble = anchor_view != nullptr;
  SetModalType(dialog_is_bubble ? ui::MODAL_TYPE_NONE : ui::MODAL_TYPE_WINDOW);
  set_fixed_width(provider->GetDistanceMetric(
      dialog_is_bubble ? views::DISTANCE_BUBBLE_PREFERRED_WIDTH
                       : views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  // Add margins for the icon plus the icon-title padding so that the dialog
  // contents align with the title text.
  set_margins(
      margins() +
      gfx::Insets(0, margins().left() + extension_misc::EXTENSION_ICON_SMALL, 0,
                  0));

  auto heading = std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_PRINTING_API_PRINT_REQUEST_BUBBLE_HEADING,
          extension_name, print_job_title, printer_name),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
  heading->SetMultiLine(true);
  heading->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  heading->SetAllowCharacterBreak(true);
  AddChildView(std::move(heading));

  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::PRINT_JOB_CONFIRMATION);
}

PrintJobConfirmationDialogView::~PrintJobConfirmationDialogView() = default;

BEGIN_METADATA(PrintJobConfirmationDialogView, views::BubbleDialogDelegateView)
END_METADATA

namespace chrome {

void ShowPrintJobConfirmationDialog(gfx::NativeWindow parent,
                                    const std::string& extension_id,
                                    const std::u16string& extension_name,
                                    const gfx::ImageSkia& extension_icon,
                                    const std::u16string& print_job_title,
                                    const std::u16string& printer_name,
                                    base::OnceCallback<void(bool)> callback) {
  PrintJobConfirmationDialogView::Show(parent, extension_id, extension_name,
                                       extension_icon, print_job_title,
                                       printer_name, std::move(callback));
}

}  // namespace chrome
