// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_BATCH_UPLOAD_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_BATCH_UPLOAD_DIALOG_VIEW_H_

#include "chrome/browser/profiles/batch_upload/batch_upload_delegate.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;

class BatchUploadDialogViewTest;

class BatchUploadDialogView : public views::DialogDelegateView {
  METADATA_HEADER(BatchUploadDialogView, views::DialogDelegateView)

 public:
  BatchUploadDialogView(const BatchUploadDialogView& other) = delete;
  BatchUploadDialogView& operator=(const BatchUploadDialogView& other) = delete;
  ~BatchUploadDialogView() override;

  static void CreateBatchUploadDialogView(
      Browser& browser,
      const std::vector<raw_ptr<const BatchUploadDataProvider>>&
          data_providers_list,
      SelectedDataTypeItemsCallback complete_callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(BatchUploadDialogViewTest,
                           OpenBatchUploadDialogViewWithClose);
  FRIEND_TEST_ALL_PREFIXES(BatchUploadDialogViewTest,
                           OpenBatchUploadDialogWithViewDestroyed);

  explicit BatchUploadDialogView(
      const std::vector<raw_ptr<const BatchUploadDataProvider>>&
          data_providers_list,
      SelectedDataTypeItemsCallback complete_callback);

  SelectedDataTypeItemsCallback complete_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_BATCH_UPLOAD_DIALOG_VIEW_H_
