// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/batch_upload_dialog_view.h"

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr base::flat_map<BatchUploadDataType,
                         std::vector<BatchUploadDataItemModel::Id>>
    kEmptySelectedMap;

const std::u16string kBatchUploadTitle = u"Save data to account";

}  // namespace

class BatchUploadDialogViewTest : public testing::Test {};

TEST_F(BatchUploadDialogViewTest, OpenBatchUploadDialogViewWithClose) {
  base::MockCallback<SelectedDataTypeItemsCallback> mock_callback;

  BatchUploadDialogView dialog_view({}, mock_callback.Get());
  EXPECT_EQ(dialog_view.GetWindowTitle(), kBatchUploadTitle);

  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);
  dialog_view.Close();
}

TEST_F(BatchUploadDialogViewTest, OpenBatchUploadDialogWithViewDestroyed) {
  base::MockCallback<SelectedDataTypeItemsCallback> mock_callback;

  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);
  {
    BatchUploadDialogView dialog_view({}, mock_callback.Get());
    EXPECT_EQ(dialog_view.GetWindowTitle(), kBatchUploadTitle);
  }
}
