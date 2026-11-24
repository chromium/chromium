// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/entry_point_display_reason.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

using internal::GetEntryPointDisplayReason;
using ::testing::Test;

const char kHttpsUrl[] = "https://www.foo.com";
const char kHttpUrl[] = "http://www.foo.com";

class EntryPointDisplayReasonTest : public Test {
 public:
  EntryPointDisplayReasonTest() {
    pref_service_.registry()->RegisterBooleanPref(prefs::kSigninAllowed, true);
    sync_service_.SetSignedOut();
    send_tab_to_self_model_.SetIsReady(false);
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

#if BUILDFLAG(IS_CHROMEOS)
// Verifies that the entry point is hidden when signed out on ChromeOS (e.g.
// Managed Guest Sessions or kiosk sessions) even when sign-in is allowed by
// prefs and the model reports ready.
TEST_F(EntryPointDisplayReasonTest, ShouldHideEntryPointIfSignedOut) {
  EXPECT_FALSE(GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                          send_tab_to_self_model(),
                                          pref_service()));

  // Also ensure a ready model does not fall through to `kInformNoTargetDevice`
  // or `kOfferFeature` when there is no signed-in account.
  send_tab_to_self_model()->SetIsReady(true);
  send_tab_to_self_model()->SetHasValidTargetDevice(false);
  EXPECT_FALSE(GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                          send_tab_to_self_model(),
                                          pref_service()));

  send_tab_to_self_model()->SetHasValidTargetDevice(true);
  EXPECT_FALSE(GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                          send_tab_to_self_model(),
                                          pref_service()));
}
#else
// Verifies that the sign-in promo is offered when the user is signed out on
// platforms that support in-browser sign-in.
TEST_F(EntryPointDisplayReasonTest, ShouldShowPromoIfSignedOut) {
  EXPECT_EQ(
      EntryPointDisplayReason::kOfferSignIn,
      GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                 send_tab_to_self_model(), pref_service()));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Verifies that the reauth promo is offered when the user is in a sign-in
// pending state (persistent auth error).
TEST_F(EntryPointDisplayReasonTest, ShouldShowPromoIfSigninPending) {
  SignIn();
  sync_service()->SetPersistentAuthError();

  EXPECT_EQ(
      EntryPointDisplayReason::kOfferReauth,
      GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                 send_tab_to_self_model(), pref_service()));
}

// Verifies that the reauth promo is hidden when sign-in is disallowed by pref.
TEST_F(EntryPointDisplayReasonTest, ShouldHideReauthPromoIfSigninDisallowed) {
  SignIn();
  sync_service()->SetPersistentAuthError();
  pref_service()->SetBoolean(prefs::kSigninAllowed, false);

  EXPECT_FALSE(GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                          send_tab_to_self_model(),
                                          pref_service()));
}

// Verifies that the reauth promo is hidden when sync is disabled by enterprise
// policy while in a sign-in pending state.
TEST_F(EntryPointDisplayReasonTest,
       ShouldHideReauthPromoIfSyncDisabledByPolicy) {
  SignIn();
  sync_service()->SetPersistentAuthError();
  sync_service()->SetAllowedByEnterprisePolicy(false);

  EXPECT_FALSE(GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                          send_tab_to_self_model(),
                                          pref_service()));
}
#else
// Verifies that the entry point is hidden on non-DICE platforms when the user
// is in a persistent auth error state.
TEST_F(EntryPointDisplayReasonTest, ShouldHideEntryPointIfSigninPending) {
  SignIn();
  sync_service()->SetPersistentAuthError();
  send_tab_to_self_model()->SetIsReady(true);

  EXPECT_FALSE(GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                          send_tab_to_self_model(),
                                          pref_service()));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Verifies that the entry point is hidden when signed out and browser sign-in
// is disallowed by `prefs::kSigninAllowed`, even if the model is ready.
TEST_F(EntryPointDisplayReasonTest, ShouldHideEntryPointIfSigninDisallowed) {
  pref_service()->SetBoolean(prefs::kSigninAllowed, false);

  EXPECT_FALSE(GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                          send_tab_to_self_model(),
                                          pref_service()));

  send_tab_to_self_model()->SetIsReady(true);
  send_tab_to_self_model()->SetHasValidTargetDevice(true);
  EXPECT_FALSE(GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                          send_tab_to_self_model(),
                                          pref_service()));
}

// Verifies that the entry point is hidden when local sync is enabled and there
// is no signed-in account, even if the model reports ready.
TEST_F(EntryPointDisplayReasonTest, ShouldHideEntryPointIfLocalSyncEnabled) {
  sync_service()->SetLocalSyncEnabled(true);
  send_tab_to_self_model()->SetIsReady(true);

  EXPECT_FALSE(GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                          send_tab_to_self_model(),
                                          pref_service()));
}

// Verifies that the entry point is hidden when sync is disabled by enterprise
// policy while signed out, even if the model reports ready.
TEST_F(EntryPointDisplayReasonTest, ShouldHidePromoIfSyncDisabledByPolicy) {
  sync_service()->SetAllowedByEnterprisePolicy(false);

  EXPECT_FALSE(GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                          send_tab_to_self_model(),
                                          pref_service()));

  send_tab_to_self_model()->SetIsReady(true);
  EXPECT_FALSE(GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                          send_tab_to_self_model(),
                                          pref_service()));
}

// Tests that the entry point is hidden if `SyncService` is null.
TEST_F(EntryPointDisplayReasonTest, ShouldHideEntryPointIfSyncServiceNull) {
  EXPECT_FALSE(
      GetEntryPointDisplayReason(GURL(kHttpsUrl), /*sync_service=*/nullptr,
                                 send_tab_to_self_model(), pref_service()));
}

// Tests that the entry point is hidden if `SendTabToSelfModel` is null.
TEST_F(EntryPointDisplayReasonTest, ShouldHideEntryPointIfModelNull) {
  EXPECT_FALSE(GetEntryPointDisplayReason(GURL(kHttpsUrl), sync_service(),
                                          /*send_tab_to_self_model=*/nullptr,
                                          pref_service()));
}

// Tests that the entry point is hidden when the model is not ready.
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
