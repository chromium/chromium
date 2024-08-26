// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILES_BATCH_UPLOAD_UI_DELEGATE_H_
#define CHROME_BROWSER_UI_PROFILES_BATCH_UPLOAD_UI_DELEGATE_H_

#include "chrome/browser/profiles/batch_upload/batch_upload_delegate.h"

class Browser;

// Implementation that creates the Batch Upload dialog UI view.
class BatchUploadUIDelegate : public BatchUploadDelegate {
 public:
  BatchUploadUIDelegate();
  ~BatchUploadUIDelegate() override;

  // BatchUploadDelegate:
  void ShowBatchUploadDialog(
      Browser* browser,
      const std::vector<raw_ptr<const BatchUploadDataProvider>>&
          data_providers_list,
      SelectedDataTypeItemsCallback complete_callback) override;

 private:
  // Implemented in
  // `chrome/browser/ui/views/profiles/batch_upload_dialog_view.h`.
  // Triggers the creation of the main view for the Batch Upload Dialog.
  void ShowBatchUploadDialogInternal(
      Browser& browser,
      const std::vector<raw_ptr<const BatchUploadDataProvider>>&
          data_providers_list,
      SelectedDataTypeItemsCallback complete_callback);
};

#endif  // CHROME_BROWSER_UI_PROFILES_BATCH_UPLOAD_UI_DELEGATE_H_
