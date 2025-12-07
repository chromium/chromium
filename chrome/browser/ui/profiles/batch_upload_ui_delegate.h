// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILES_BATCH_UPLOAD_UI_DELEGATE_H_
#define CHROME_BROWSER_UI_PROFILES_BATCH_UPLOAD_UI_DELEGATE_H_

#include "chrome/browser/profiles/batch_upload/batch_upload_delegate.h"
#include "components/sync/service/local_data_description.h"

class Browser;

// Implementation that creates the Batch Upload dialog UI view.
class BatchUploadUIDelegate : public BatchUploadDelegate {
 public:
  BatchUploadUIDelegate();
  ~BatchUploadUIDelegate() override;

  // BatchUploadDelegate:
  void ShowBatchUploadDialog(
      Browser* browser,
      std::vector<syncer::LocalDataDescription> local_data_description_list,
      BatchUploadService::EntryPoint entry_point,
      BatchUploadSelectedDataTypeItemsCallback complete_callback) override;

 private:
  // Implemented in
  // `chrome/browser/ui/views/profiles/batch_upload_dialog_view.h`.
  // Triggers the creation of the main view for the Batch Upload Dialog.
  void ShowBatchUploadDialogInternal(
      Browser& browser,
      std::vector<syncer::LocalDataDescription> local_data_description_list,
      BatchUploadService::EntryPoint entry_point,
      BatchUploadSelectedDataTypeItemsCallback complete_callback);
};

#endif  // CHROME_BROWSER_UI_PROFILES_BATCH_UPLOAD_UI_DELEGATE_H_
