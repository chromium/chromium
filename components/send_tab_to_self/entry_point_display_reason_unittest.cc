// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/entry_point_display_reason.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/test_send_tab_to_self_model.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

using internal::GetEntryPointDisplayReason;

const char kHttpsUrl[] = "https://www.foo.com";
const char kHttpUrl[] = "http://www.foo.com";

class FakeSendTabToSelfModel : public TestSendTabToSelfModel {
 public:
  FakeSendTabToSelfModel() = default;
  ~FakeSendTabToSelfModel() override = default;

  void SetIsReady(bool is_ready) { is_ready_ = is_ready; }
  void SetHasValidTargetDevice(bool has_valid_target_device) {
    if (has_valid_target_device) {
      DCHECK(is_ready_) << "Target devices are only known if the model's ready";
    }
    has_valid_target_device_ = has_valid_target_device;
  }

  bool IsReady() override { return is_ready_; }
  bool HasValidTargetDevice() override { return has_valid_target_device_; }

 private:
  bool is_ready_ = false;
  bool has_valid_target_device_ = false;
};

class EntryPointDisplayReasonTest : public ::testing::Test {
 public:
  EntryPointDisplayReasonTest() {
    pref_service_.registry()->RegisterBooleanPref(prefs::kSigninAllowed, true);
    sync_service_.SetSignedOut();
  }

  syncer::TestSyncService* sync_service() { return &sync_service_; }
  FakeSendTabToSelfModel* send_tab_to_self_model() {
    return &send_tab_to_self_model_;
  }
  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

  void SignIn() { sync_service_.SetSignedIn(signin::ConsentLevel::kSignin); }

 private:
  syncer::TestSyncService sync_service_;
  FakeSendTabToSelfModel send_tab_to_self_model_;
  TestingPrefServiceSimple pref_service_;
};

// The promo isn't supported on Lacros yet.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(EntryPointDisplayReasonTest, ShouldShowPromoIfSignedOut) {
  EXPECT_EQ(
      EntryPointDisplayReason::kOfferSignIn,
      GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                 send_tab_to_self_model(), pref_service()));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_F(EntryPointDisplayReasonTest, ShouldHidePromoIfSyncDisabledByPolicy) {
  sync_service()->SetAllowedByEnterprisePolicy(false);

  EXPECT_FALSE(GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                          send_tab_to_self_model(),
                                          pref_service()));
}

TEST_F(EntryPointDisplayReasonTest, ShouldHideEntryPointIfModelNotReady) {
  SignIn();
  send_tab_to_self_model()->SetIsReady(false);
  send_tab_to_self_model()->SetHasValidTargetDevice(false);

  EXPECT_FALSE(GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                          send_tab_to_self_model(),
                                          pref_service()));
}

TEST_F(EntryPointDisplayReasonTest, ShouldShowPromoIfHasNoValidTargetDevice) {
  SignIn();
  send_tab_to_self_model()->SetIsReady(true);
  send_tab_to_self_model()->SetHasValidTargetDevice(false);

  EXPECT_EQ(
      EntryPointDisplayReason::kInformNoTargetDevice,
      GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                 send_tab_to_self_model(), pref_service()));
}

TEST_F(EntryPointDisplayReasonTest, ShouldOnlyOfferFeatureIfHttpOrHttps) {
  SignIn();
  send_tab_to_self_model()->SetIsReady(true);
  send_tab_to_self_model()->SetHasValidTargetDevice(true);

  EXPECT_EQ(
      EntryPointDisplayReason::kOfferFeature,
      GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                 send_tab_to_self_model(), pref_service()));

  EXPECT_EQ(
      EntryPointDisplayReason::kOfferFeature,
      GetEntryPointDisplayReason(GURL(kHttpUrl), sync_service(),
                                 send_tab_to_self_model(), pref_service()));

  EXPECT_FALSE(GetEntryPointDisplayReason(GURL("192.168.0.0"), sync_service(),
                                          send_tab_to_self_model(),
                                          pref_service()));

  EXPECT_FALSE(
      GetEntryPointDisplayReason(GURL("chrome-untrusted://url"), sync_service(),
                                 send_tab_to_self_model(), pref_service()));

  EXPECT_FALSE(
      GetEntryPointDisplayReason(GURL("chrome://flags"), sync_service(),
                                 send_tab_to_self_model(), pref_service()));

  EXPECT_FALSE(
      GetEntryPointDisplayReason(GURL("tel:07399999999"), sync_service(),
                                 send_tab_to_self_model(), pref_service()));
}

}  // namespace

}  // namespace send_tab_to_self
