// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/android/data_sharing_network_loader_android.h"

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "components/data_sharing/test_support/mock_data_sharing_network_loader.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/android/gurl_android.h"

using ::testing::_;

namespace data_sharing {

class DataSharingNetworkLoaderAndroidTest : public testing::Test {
 public:
  DataSharingNetworkLoaderAndroidTest() = default;
  ~DataSharingNetworkLoaderAndroidTest() override = default;

  void SetUp() override {
    network_loader_android_ = std::make_unique<DataSharingNetworkLoaderAndroid>(
        &mock_network_loader_);
  }

 protected:
  MockDataSharingNetworkLoader mock_network_loader_;
  std::unique_ptr<DataSharingNetworkLoaderAndroid> network_loader_android_;
};

TEST_F(DataSharingNetworkLoaderAndroidTest, LoadUrl) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<std::string> scopes;

  EXPECT_CALL(mock_network_loader_, LoadUrl(_, _, _, _, _)).Times(1);
  network_loader_android_->LoadUrl(
      env, url::GURLAndroid::EmptyGURL(env),
      base::android::ToJavaArrayOfStrings(env, scopes),
      base::android::ToJavaByteArray(env, "foo"),
      TRAFFIC_ANNOTATION_FOR_TESTS.unique_id_hash_code, nullptr);
}

}  // namespace data_sharing
