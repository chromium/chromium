// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/batch_upload_dialog_view.h"

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profiles/batch_upload_ui_delegate.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

BatchUploadDialogView::~BatchUploadDialogView() {
  // If the view was closed without a user action, run the callback as if it was
  // cancelled (empty result).
  if (complete_callback_) {
    std::move(complete_callback_).Run({});
  }
}

// static
void BatchUploadDialogView::CreateBatchUploadDialogView(
    Browser& browser,
    const std::vector<raw_ptr<const BatchUploadDataProvider>>&
        data_providers_list,
    SelectedDataTypeItemsCallback complete_callback) {
  std::unique_ptr<BatchUploadDialogView> dialog_view =
      base::WrapUnique(new BatchUploadDialogView(data_providers_list,
                                                 std::move(complete_callback)));

  gfx::NativeWindow window = browser.tab_strip_model()
                                 ->GetActiveWebContents()
                                 ->GetTopLevelNativeWindow();

  views::Widget* widget = constrained_window::CreateBrowserModalDialogViews(
      std::move(dialog_view), window);
  widget->Show();
}

BatchUploadDialogView::BatchUploadDialogView(
    const std::vector<raw_ptr<const BatchUploadDataProvider>>&
        data_providers_list,
    SelectedDataTypeItemsCallback complete_callback)
    : complete_callback_(std::move(complete_callback)) {
  // Temporary hardcoded name.
  SetTitle(u"Save data to account");
  SetModalType(ui::mojom::ModalType::kWindow);
  // Temporary.
  SetShowCloseButton(true);
}

// BatchUploadUIDelegate -------------------------------------------------------

void BatchUploadUIDelegate::ShowBatchUploadDialogInternal(
    Browser& browser,
    const std::vector<raw_ptr<const BatchUploadDataProvider>>&
        data_providers_list,
    SelectedDataTypeItemsCallback complete_callback) {
  BatchUploadDialogView::CreateBatchUploadDialogView(
      browser, data_providers_list, std::move(complete_callback));
}

BEGIN_METADATA(BatchUploadDialogView)
END_METADATA
