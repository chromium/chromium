// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_local_data_batch_uploader.h"

#include "base/test/mock_callback.h"
#include "components/sync/service/local_data_description.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

class PasswordLocalDataBatchUploaderTest : public ::testing::Test {};

TEST_F(PasswordLocalDataBatchUploaderTest, Sanity) {
  base::MockCallback<base::OnceCallback<void(syncer::LocalDataDescription)>>
      callback;
  PasswordLocalDataBatchUploader uploader;
  EXPECT_CALL(callback, Run);

  uploader.GetLocalDataDescription(callback.Get());
}

}  // namespace
}  // namespace password_manager
