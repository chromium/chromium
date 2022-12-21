// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/real_time_url_checks_allowlist.h"

#include <string.h>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "components/grit/components_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace safe_browsing {

// This test validates the contents of the backup resource file,
// real_time_url_checks_allowlist.pb
class RealTimeUrlChecksAllowlistResourceFileTest : public testing::Test {
 protected:
  RealTimeUrlChecksAllowlistResourceFileTest() = default;
  ~RealTimeUrlChecksAllowlistResourceFileTest() override = default;

  void CheckValidAllowlist() {
    for (auto allowed_url : allowlisted_) {
      auto full_url = GURL("https://" + allowed_url);
      EXPECT_EQ(allowlist_.IsInAllowlist(full_url),
                RealTimeUrlChecksAllowlist::IsInAllowlistResult::kInAllowlist);
    }

    for (auto not_allowed_url : not_allowlisted_) {
      auto full_url = GURL("https://" + not_allowed_url);
      EXPECT_EQ(
          allowlist_.IsInAllowlist(full_url),
          RealTimeUrlChecksAllowlist::IsInAllowlistResult::kNotInAllowlist);
    }
  }

  // Use this function so that PopulateAllowlistFromBinaryPb() can be
  // private
  RealTimeUrlChecksAllowlist::PopulateResult CallPopulateAllowlistFromBinaryPb(
      std::string binary_pb) {
    return allowlist_.PopulateAllowlistFromBinaryPb(binary_pb);
  }

 protected:
  RealTimeUrlChecksAllowlist allowlist_;
  std::vector<std::string> allowlisted_ = {
      "www.facebook.com", "www.facebook.com/friends", "www.wikipedia.org",
      "www.google.com/images", "doubleclick.net"};
  std::vector<std::string> not_allowlisted_ = {
      "www.sites.google.com",
      "www.evil.com",
  };
};

TEST_F(RealTimeUrlChecksAllowlistResourceFileTest,
       PopulateAllowlistFromBinaryPbResourceFile) {
  base::AutoLock lock(allowlist_.lock_);
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  std::string valid_pb =
      bundle.LoadDataResourceString(IDR_REAL_TIME_URL_CHECKS_ALLOWLIST_PB);
  EXPECT_EQ(RealTimeUrlChecksAllowlist::PopulateResult::kSuccess,
            CallPopulateAllowlistFromBinaryPb(valid_pb));
  base::AutoUnlock unlock(allowlist_.lock_);
  CheckValidAllowlist();
}

}  // namespace safe_browsing
