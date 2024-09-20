// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_BATCH_UPLOAD_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_BATCH_UPLOAD_DIALOG_VIEW_H_

#include "chrome/browser/profiles/batch_upload/batch_upload_delegate.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;
class Profile;

class BatchUploadDialogViewBrowserTest;

namespace views {
class WebView;
}  // namespace views

// Native dialog view that holds the web ui component for the Batch Upload ui.
// It needs to adapt the height size based on the web ui content that is
// displayed, which is dynamic.
class BatchUploadDialogView : public views::DialogDelegateView {
  METADATA_HEADER(BatchUploadDialogView, views::DialogDelegateView)

 public:
  BatchUploadDialogView(const BatchUploadDialogView& other) = delete;
  BatchUploadDialogView& operator=(const BatchUploadDialogView& other) = delete;
  ~BatchUploadDialogView() override;

  // Creates the dialog view and registers as a modal view.
  // The created dialog view is owned by the views system.
  static BatchUploadDialogView* CreateBatchUploadDialogView(
      Browser& browser,
      const std::vector<raw_ptr<const BatchUploadDataProvider>>&
          data_providers_list,
      SelectedDataTypeItemsCallback complete_callback);

 private:
  friend class BatchUploadDialogViewBrowserTest;

  FRIEND_TEST_ALL_PREFIXES(BatchUploadDialogViewBrowserTest,
                           OpenBatchUploadDialogViewWithCloseAction);
  FRIEND_TEST_ALL_PREFIXES(BatchUploadDialogViewBrowserTest,
                           OpenBatchUploadDialogViewWithDestroyed);

  explicit BatchUploadDialogView(
      Profile* profile,
      const std::vector<raw_ptr<const BatchUploadDataProvider>>&
          data_providers_list,
      SelectedDataTypeItemsCallback complete_callback);

  // Callback to properly resize the view based on the loaded web ui content.
  // Also shows the widget.
  void SetHeightAndShowWidget(int height);

  // Callback to receive the selected items from the web ui view.
  // Empty list means the dialog was closed without a move item request.
  void OnDialogSelectionMade(
      const base::flat_map<BatchUploadDataType,
                           std::vector<BatchUploadDataItemModel::Id>>&
          selected_map);

  // Callback used as a clearing method whenever the view is being closed. Used
  // to clear any data until the view is actually closed, given that closing the
  // view fully is asynchronous.
  void OnClose();

  SelectedDataTypeItemsCallback complete_callback_;

  raw_ptr<views::WebView> web_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_BATCH_UPLOAD_DIALOG_VIEW_H_
