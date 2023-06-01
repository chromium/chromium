// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/identifiers/profile_id_service.h"

#include <utility>

#include "base/base64url.h"
#include "base/hash/sha1.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/enterprise/browser/identifiers/identifiers_prefs.h"
#include "components/enterprise/browser/identifiers/mock_profile_id_delegate.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

namespace enterprise {

namespace {

constexpr char kFakeProfileGUID[] = "fake-guid";
constexpr char kFakeDeviceId[] = "fake-id";
constexpr char kProfildeIdErrorHistogramName[] =
    "Enterprise.ProfileIdentifier.Error";
constexpr char kProfildeIdStatusHistogramName[] =
    "Enterprise.ProfileIdentifier.Status";

}  // namespace

using test::MockProfileIdDelegate;

class ProfileIdServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    auto mock_delegate = std::make_unique<MockProfileIdDelegate>();
    mock_delegate_ = mock_delegate.get();
    RegisterIdentifiersProfilePrefs(profile_prefs_.registry());
    service_ = std::make_unique<ProfileIdService>(std::move(mock_delegate),
                                                  &profile_prefs_);
  }

  raw_ptr<MockProfileIdDelegate, DanglingUntriaged> mock_delegate_ = nullptr;
  TestingPrefServiceSimple profile_prefs_;
  std::unique_ptr<ProfileIdService> service_;
  base::HistogramTester histogram_tester_;
};

// Tests that no profile ID is retrieved if the profile GUID is empty.
TEST_F(ProfileIdServiceTest, GetProfileId_Failure_NoProfileGuid) {
  EXPECT_FALSE(service_->GetProfileId());
  histogram_tester_.ExpectUniqueSample(
      kProfildeIdErrorHistogramName,
      ProfileIdService::Error::kGetProfileGUIDFailure, 1);
  histogram_tester_.ExpectUniqueSample(kProfildeIdStatusHistogramName, false,
                                       1);
}

// Tests that no profile ID is retrieved if the device ID is empty.
TEST_F(ProfileIdServiceTest, GetProfileId_Failure_NoDeviceId) {
  profile_prefs_.SetString(kProfileGUIDPref, kFakeProfileGUID);
  EXPECT_CALL(*mock_delegate_, GetDeviceId()).WillOnce(Return(""));
  EXPECT_FALSE(service_->GetProfileId());
  histogram_tester_.ExpectUniqueSample(
      kProfildeIdErrorHistogramName,
      ProfileIdService::Error::kGetDeviceIdFailure, 1);
  histogram_tester_.ExpectUniqueSample(kProfildeIdStatusHistogramName, false,
                                       1);
}

// Tests a successful profile ID generation.
TEST_F(ProfileIdServiceTest, GetProfileId_Success) {
  profile_prefs_.SetString(kProfileGUIDPref, kFakeProfileGUID);
  EXPECT_CALL(*mock_delegate_, GetDeviceId()).WillOnce(Return(kFakeDeviceId));

  std::string encoded_string;
  base::Base64UrlEncode(base::SHA1HashString(std::string(kFakeProfileGUID) +
                                             std::string(kFakeDeviceId)),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_string);
  EXPECT_EQ(encoded_string, service_->GetProfileId());
  histogram_tester_.ExpectTotalCount(kProfildeIdErrorHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(kProfildeIdStatusHistogramName, true, 1);
}

}  // namespace enterprise
