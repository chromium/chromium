// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_local_data_batch_uploader.h"

#include "base/test/mock_callback.h"
#include "components/sync/service/local_data_description.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reading_list {
namespace {

class ReadingListLocalDataBatchUploaderTest : public ::testing::Test {};

TEST_F(ReadingListLocalDataBatchUploaderTest, Sanity) {
  base::MockCallback<base::OnceCallback<void(syncer::LocalDataDescription)>>
      callback;
  ReadingListLocalDataBatchUploader uploader;
  EXPECT_CALL(callback, Run);

  uploader.GetLocalDataDescription(callback.Get());
}

}  // namespace
}  // namespace reading_list
