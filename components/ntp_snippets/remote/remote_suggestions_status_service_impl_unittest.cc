// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_status_service_impl.h"

#include <memory>

#include "base/bind.h"
#include "build/build_config.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

namespace {
const char kTestPrefName[] = "search_suggestions.test_name";

}  // namespace

class RemoteSuggestionsStatusServiceImplTest : public ::testing::Test {
 public:
  RemoteSuggestionsStatusServiceImplTest()
      : last_status_(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN) {
    RemoteSuggestionsStatusServiceImpl::RegisterProfilePrefs(
        utils_.pref_service()->registry());

    // Registering additional test preference for testing serch suggestion based
    // feature disabling.
    utils_.pref_service()->registry()->RegisterBooleanPref(kTestPrefName, true);
  }

  // |empty_additional_pref| indicates whether the service is created without an
  // additional pref.
  std::unique_ptr<RemoteSuggestionsStatusServiceImpl> MakeService(
      bool empty_additional_pref) {
    auto service = std::make_unique<RemoteSuggestionsStatusServiceImpl>(
        false, utils_.pref_service(),
        empty_additional_pref ? std::string() : kTestPrefName);
    service->Init(base::BindRepeating(
        &RemoteSuggestionsStatusServiceImplTest::OnStatusChange,
        base::Unretained(this)));
    return service;
  }

  RemoteSuggestionsStatus last_status() const { return last_status_; }

 protected:
  void OnStatusChange(RemoteSuggestionsStatus old_status,
                      RemoteSuggestionsStatus new_status) {
    last_status_ = new_status;
  }

  RemoteSuggestionsStatus last_status_;
  test::RemoteSuggestionsTestUtils utils_;
};

TEST_F(RemoteSuggestionsStatusServiceImplTest, NoSigninNeeded) {
  auto service = MakeService(/*empty_additional_pref=*/false);

  // By default, no signin is required.
  EXPECT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT, last_status());

  // Signin should cause a state change.
  service->OnSignInStateChanged(/*has_signed_in=*/true);
  EXPECT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN, last_status());
}

TEST_F(RemoteSuggestionsStatusServiceImplTest, DisabledViaPref) {
  auto service = MakeService(/*empty_additional_pref=*/false);

  // The default test setup is signed out. The service is enabled.
  ASSERT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT, last_status());

  // Once the enabled pref is set to false, we should be disabled.
  utils_.pref_service()->SetBoolean(prefs::kEnableSnippets, false);
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED, last_status());

  // The state should not change, even though a signin has occurred.
  service->OnSignInStateChanged(/*has_signed_in=*/true);
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED, last_status());
}

TEST_F(RemoteSuggestionsStatusServiceImplTest, DisabledViaAdditionalPref) {
  auto service = MakeService(/*empty_additional_pref=*/false);

  // The default test setup is signed out. The service is enabled.
  ASSERT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT, last_status());

  // Once the additional pref is set to false, we should be disabled.
  utils_.pref_service()->SetBoolean(kTestPrefName, false);
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED, last_status());

  // The state should not change, even though a signin has occurred.
  service->OnSignInStateChanged(/*has_signed_in=*/true);
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED, last_status());
}

TEST_F(RemoteSuggestionsStatusServiceImplTest, EnabledAfterListFolded) {
  auto service = MakeService(/*empty_additional_pref=*/true);
  // By default, the articles list should be visible.
  EXPECT_TRUE(utils_.pref_service()->GetBoolean(prefs::kArticlesListVisible));

  // The default test setup is signed out. The service is enabled.
  ASSERT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT, last_status());

  // When the user toggles the visibility of articles list in UI off the service
  // should still be enabled until the end of the session.
  utils_.pref_service()->SetBoolean(prefs::kArticlesListVisible, false);
  EXPECT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT, last_status());

  // Signin should cause a state change.
  service->OnSignInStateChanged(/*has_signed_in=*/true);
  EXPECT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN, last_status());
}

TEST_F(RemoteSuggestionsStatusServiceImplTest, DisabledWhenListFoldedOnStart) {
  utils_.pref_service()->SetBoolean(prefs::kArticlesListVisible, false);
  auto service = MakeService(/*empty_additional_pref=*/true);

  // The state should be disabled when starting with no list shown.
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED, last_status());

  // The state should not change, even though a signin has occurred.
  service->OnSignInStateChanged(/*has_signed_in=*/true);
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED, last_status());
}

TEST_F(RemoteSuggestionsStatusServiceImplTest, EnablingAfterFoldedStart) {
  utils_.pref_service()->SetBoolean(prefs::kArticlesListVisible, false);
  auto service = MakeService(/*empty_additional_pref=*/true);

  // The state should be disabled when starting with no list shown.
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED, last_status());

  // When the user toggles the visibility of articles list in UI on, the service
  // should get enabled.
  utils_.pref_service()->SetBoolean(prefs::kArticlesListVisible, true);
  EXPECT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT, last_status());

  // Signin should cause a state change.
  service->OnSignInStateChanged(/*has_signed_in=*/true);
  EXPECT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN, last_status());
}

TEST_F(RemoteSuggestionsStatusServiceImplTest,
       EnablingAfterFoldedStartSignedIn) {
  utils_.pref_service()->SetBoolean(prefs::kArticlesListVisible, false);
  auto service = MakeService(/*empty_additional_pref=*/true);

  // Signin should not cause a state change, because UI is not visible.
  service->OnSignInStateChanged(/*has_signed_in=*/true);
  EXPECT_EQ(RemoteSuggestionsStatus::EXPLICITLY_DISABLED, last_status());

  // When the user toggles the visibility of articles list in UI on, the service
  // should get enabled.
  utils_.pref_service()->SetBoolean(prefs::kArticlesListVisible, true);
  EXPECT_EQ(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN, last_status());
}

}  // namespace ntp_snippets
