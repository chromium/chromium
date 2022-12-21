// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/real_time_url_checks_allowlist.h"
#include "components/safe_browsing/android/proto/realtimeallowlist.pb.h"

#include <string.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "crypto/sha2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::NiceMock;

namespace safe_browsing {

class RealTimeUrlChecksAllowlistTest : public testing::Test {
 protected:
  RealTimeUrlChecksAllowlistTest() = default;
  ~RealTimeUrlChecksAllowlistTest() override = default;

  void SetUp() override {
    allowlisted_.push_back("example.com/");
    allowlisted_.push_back("example.test/");
    allowlist_.SetMinimumEntryCountForTesting(allowlisted_.size());

    // Create a dangling hash string such that the length is not divisible
    // by the truncated hash length (which is 16). A dangling hash indicates
    // that the url hashes are in an invalid format.
    for (int i = 0; i < 1601; ++i)
      dangling_hash_str_ += "A";

    valid_hca_.set_version_id(2);
    valid_hca_.set_scheme_id(0);
    std::string combined_hashes = "";
    for (auto url_string : allowlisted_) {
      auto url_hash = crypto::SHA256HashString(url_string);
      auto url_hash_sub = url_hash.substr(0, 16);
      combined_hashes += url_hash_sub;
    }
    valid_hca_.set_url_hashes(combined_hashes);
    serialized_valid_hca_ = valid_hca_.SerializeAsString();
  }

  // Use this function to call PopulateFromResourceBundle()
  // so that we can have this function protected
  void CallPopulateFromResourceBundle() {
    allowlist_.PopulateFromResourceBundle();
  }

  // Similar to CallPopulateFromResourceBundle(), use this
  // function so that PopulateAllowlistFromBinaryPb() can be
  // protected
  RealTimeUrlChecksAllowlist::PopulateResult CallPopulateAllowlistFromBinaryPb(
      std::string binary_pb) {
    return allowlist_.PopulateAllowlistFromBinaryPb(binary_pb);
  }

  HighConfidenceAllowlist CreateHighConfidenceAllowlist(
      int version_id,
      int scheme_id,
      std::string url_hashes) {
    HighConfidenceAllowlist allowlist;
    allowlist.set_version_id(version_id);
    allowlist.set_scheme_id(scheme_id);
    allowlist.set_url_hashes(url_hashes);
    return allowlist;
  }

  void CheckPopulateHistogramResult(
      std::string function_name,
      RealTimeUrlChecksAllowlist::PopulateResult result,
      int count) {
    histogram_tester_.ExpectBucketCount(
        "SafeBrowsing.Android.RealTimeAllowlist.Populate." + function_name +
            "Result",
        result, count);
  }

 protected:
  RealTimeUrlChecksAllowlist allowlist_;
  HighConfidenceAllowlist valid_hca_;
  base::HistogramTester histogram_tester_;
  GURL fb_url = GURL("https://www.facebook.com");
  GURL example_url = GURL("https://www.example.com");
  std::vector<std::string> allowlisted_;
  std::string dangling_hash_str_;
  std::string serialized_valid_hca_;
};

TEST_F(RealTimeUrlChecksAllowlistTest, UnpackResourceBundle) {
  CallPopulateFromResourceBundle();
  CheckPopulateHistogramResult(
      "ResourceBundle", RealTimeUrlChecksAllowlist::PopulateResult::kSuccess,
      1);
}

TEST_F(RealTimeUrlChecksAllowlistTest,
       PopulateValidAllowlistThroughDynamicUpdate) {
  allowlist_.PopulateFromDynamicUpdate(serialized_valid_hca_);
  CheckPopulateHistogramResult(
      "DynamicUpdate", RealTimeUrlChecksAllowlist::PopulateResult::kSuccess, 1);
  for (auto url : allowlisted_) {
    auto full_url = GURL("https://" + url);
    EXPECT_EQ(allowlist_.IsInAllowlist(full_url),
              RealTimeUrlChecksAllowlist::IsInAllowlistResult::kInAllowlist);
  }
  auto bad_example_url = GURL("https://www.bad_example.com");
  EXPECT_EQ(allowlist_.IsInAllowlist(bad_example_url),
            RealTimeUrlChecksAllowlist::IsInAllowlistResult::kNotInAllowlist);
}

TEST_F(RealTimeUrlChecksAllowlistTest,
       PopulateThroughDynamicAfterPopulateThroughResource) {
  CallPopulateFromResourceBundle();
  CheckPopulateHistogramResult(
      "ResourceBundle", RealTimeUrlChecksAllowlist::PopulateResult::kSuccess,
      1);
  EXPECT_EQ(allowlist_.IsInAllowlist(fb_url),
            RealTimeUrlChecksAllowlist::IsInAllowlistResult::kInAllowlist);
  EXPECT_EQ(allowlist_.IsInAllowlist(example_url),
            RealTimeUrlChecksAllowlist::IsInAllowlistResult::kNotInAllowlist);

  // Make sure the new version id is bigger than the resource file's version
  HighConfidenceAllowlist new_allowlist = CreateHighConfidenceAllowlist(
      std::numeric_limits<int>::max(), valid_hca_.scheme_id(),
      valid_hca_.url_hashes());
  allowlist_.PopulateFromDynamicUpdate(new_allowlist.SerializeAsString());
  CheckPopulateHistogramResult(
      "DynamicUpdate", RealTimeUrlChecksAllowlist::PopulateResult::kSuccess, 1);
  EXPECT_EQ(allowlist_.IsInAllowlist(fb_url),
            RealTimeUrlChecksAllowlist::IsInAllowlistResult::kNotInAllowlist);
  EXPECT_EQ(allowlist_.IsInAllowlist(example_url),
            RealTimeUrlChecksAllowlist::IsInAllowlistResult::kInAllowlist);
}

TEST_F(RealTimeUrlChecksAllowlistTest, PopulateThroughDynamicTwice) {
  allowlist_.PopulateFromDynamicUpdate(serialized_valid_hca_);
  CheckPopulateHistogramResult(
      "DynamicUpdate", RealTimeUrlChecksAllowlist::PopulateResult::kSuccess, 1);
  valid_hca_.set_version_id(3);
  allowlist_.PopulateFromDynamicUpdate(valid_hca_.SerializeAsString());
  EXPECT_EQ(allowlist_.IsInAllowlist(fb_url),
            RealTimeUrlChecksAllowlist::IsInAllowlistResult::kNotInAllowlist);
  EXPECT_EQ(allowlist_.IsInAllowlist(example_url),
            RealTimeUrlChecksAllowlist::IsInAllowlistResult::kInAllowlist);
}

TEST_F(RealTimeUrlChecksAllowlistTest,
       FailedDynamicUpdateStillProvidesResourceFileAllowlist) {
  CallPopulateFromResourceBundle();
  HighConfidenceAllowlist invalid_hca;
  allowlist_.PopulateFromDynamicUpdate(invalid_hca.SerializeAsString());
  EXPECT_EQ(allowlist_.IsInAllowlist(fb_url),
            RealTimeUrlChecksAllowlist::IsInAllowlistResult::kInAllowlist);
}

TEST_F(RealTimeUrlChecksAllowlistTest,
       FailedDynamicUpdateStillProvidesDynamicAllowlist) {
  allowlist_.PopulateFromDynamicUpdate(serialized_valid_hca_);
  HighConfidenceAllowlist invalid_hca;
  allowlist_.PopulateFromDynamicUpdate(invalid_hca.SerializeAsString());
  EXPECT_EQ(allowlist_.IsInAllowlist(example_url),
            RealTimeUrlChecksAllowlist::IsInAllowlistResult::kInAllowlist);
}

TEST_F(RealTimeUrlChecksAllowlistTest,
       PopulateEmptyAllowlistThroughDynamicUpdate) {
  allowlist_.PopulateFromDynamicUpdate(std::string());
  CheckPopulateHistogramResult(
      "DynamicUpdate", RealTimeUrlChecksAllowlist::PopulateResult::kFailedEmpty,
      1);
  auto url = GURL("https://www.facebook/com/friends");
  EXPECT_EQ(
      allowlist_.IsInAllowlist(url),
      RealTimeUrlChecksAllowlist::IsInAllowlistResult::kAllowlistUnavailable);
}

TEST_F(RealTimeUrlChecksAllowlistTest, PopulateAllowlistFromBinaryPbBadProtos) {
  base::AutoLock lock(allowlist_.lock_);
  EXPECT_EQ(RealTimeUrlChecksAllowlist::PopulateResult::kFailedEmpty,
            CallPopulateAllowlistFromBinaryPb(std::string()));
  EXPECT_EQ(RealTimeUrlChecksAllowlist::PopulateResult::kFailedProtoParse,
            CallPopulateAllowlistFromBinaryPb("foobar"));
}

TEST_F(RealTimeUrlChecksAllowlistTest,
       PopulateAllowlistFromBinaryPbMissingFields) {
  base::AutoLock lock(allowlist_.lock_);
  HighConfidenceAllowlist hca_missing_version;
  hca_missing_version.set_scheme_id(0);
  hca_missing_version.set_url_hashes("");
  EXPECT_EQ(RealTimeUrlChecksAllowlist::PopulateResult::kFailedMissingVersionId,
            CallPopulateAllowlistFromBinaryPb(
                hca_missing_version.SerializeAsString()));

  HighConfidenceAllowlist hca_missing_scheme;
  hca_missing_scheme.set_version_id(1);
  hca_missing_scheme.set_url_hashes("");
  EXPECT_EQ(RealTimeUrlChecksAllowlist::PopulateResult::kFailedMissingSchemeId,
            CallPopulateAllowlistFromBinaryPb(
                hca_missing_scheme.SerializeAsString()));

  HighConfidenceAllowlist hca_missing_url_hashes;
  hca_missing_url_hashes.set_version_id(1);
  hca_missing_url_hashes.set_scheme_id(0);
  EXPECT_EQ(RealTimeUrlChecksAllowlist::PopulateResult::kFailedMissingUrlHashes,
            CallPopulateAllowlistFromBinaryPb(
                hca_missing_url_hashes.SerializeAsString()));
}

TEST_F(RealTimeUrlChecksAllowlistTest,
       PopulateAllowlistFromBinaryPbInvalidFields) {
  base::AutoLock lock(allowlist_.lock_);
  HighConfidenceAllowlist hca_invalid_hashes =
      CreateHighConfidenceAllowlist(1, 0, std::string());
  EXPECT_EQ(RealTimeUrlChecksAllowlist::PopulateResult::kFailedEmptyUrlHashes,
            CallPopulateAllowlistFromBinaryPb(
                hca_invalid_hashes.SerializeAsString()));
  hca_invalid_hashes.set_url_hashes("hashhashhashhash");
  EXPECT_EQ(
      RealTimeUrlChecksAllowlist::PopulateResult::kFailedTooFewAllowlistEntries,
      CallPopulateAllowlistFromBinaryPb(
          hca_invalid_hashes.SerializeAsString()));
  hca_invalid_hashes.set_url_hashes(dangling_hash_str_);
  EXPECT_EQ(RealTimeUrlChecksAllowlist::PopulateResult::kFailedDanglingHash,
            CallPopulateAllowlistFromBinaryPb(
                hca_invalid_hashes.SerializeAsString()));

  HighConfidenceAllowlist hca_invalid_version =
      CreateHighConfidenceAllowlist(-1, 0, "hashhashhashhash");
  EXPECT_EQ(RealTimeUrlChecksAllowlist::PopulateResult::kSkippedEqualVersionId,
            CallPopulateAllowlistFromBinaryPb(
                hca_invalid_version.SerializeAsString()));

  HighConfidenceAllowlist hca_invalid_scheme =
      CreateHighConfidenceAllowlist(1, 1, "hashhashhashhash");
  EXPECT_EQ(RealTimeUrlChecksAllowlist::PopulateResult::kSkippedInvalidSchemeId,
            CallPopulateAllowlistFromBinaryPb(
                hca_invalid_scheme.SerializeAsString()));
}

}  // namespace safe_browsing
