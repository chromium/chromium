// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_local_data_batch_uploader.h"

#include "base/test/mock_callback.h"
#include "components/sync/service/local_data_description.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_bookmarks {
namespace {

class BookmarkLocalDataBatchUploaderTest : public ::testing::Test {};

TEST_F(BookmarkLocalDataBatchUploaderTest, Sanity) {
  base::MockCallback<base::OnceCallback<void(syncer::LocalDataDescription)>>
      callback;
  BookmarkLocalDataBatchUploader uploader;
  EXPECT_CALL(callback, Run);

  uploader.GetLocalDataDescription(callback.Get());
}

}  // namespace
}  // namespace sync_bookmarks
