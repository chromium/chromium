// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_response_params.h"

#include <memory>

#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

TEST(DiceResponseParamsTest, IsValidSignin) {
  DiceResponseParams params;
  EXPECT_FALSE(params.IsValid());  // std::monostate is not valid.

  // SIGNIN
  DiceResponseParams::SigninInfo* signin_info =
      &params.data.emplace<DiceResponseParams::SigninInfo>();
  EXPECT_FALSE(params.IsValid());  // Empty accounts.

  signin_info->AddAccount(
      {{GaiaId("id"), "email", 0}, "code", false, "binding", false});
  // One account: IsValid is true even with invalid metadata because it falls
  // back to the only account.
  EXPECT_TRUE(params.IsValid());

  signin_info->set_linked_accounts_metadata(
      DiceResponseParams::SigninInfo::LinkedAccountsMetadata{
          .primary_is_connected = Tribool::kTrue,
          .initiator_id = GaiaId("unknown")});
  // One account: IsValid is still true because it falls back to the only
  // account even if the initiator_id in metadata doesn't match.
  EXPECT_TRUE(params.IsValid());

  // Add a second account: IsValid becomes false because initiator is "unknown"
  // and cannot be resolved among multiple accounts.
  signin_info->AddAccount(
      {{GaiaId("id2"), "email2", 0}, "code2", false, "binding2", false});
  EXPECT_FALSE(params.IsValid());

  signin_info->set_linked_accounts_metadata(
      DiceResponseParams::SigninInfo::LinkedAccountsMetadata{
          .primary_is_connected = Tribool::kTrue,
          .initiator_id = GaiaId("id2")});
  EXPECT_TRUE(params.IsValid());
}

TEST(DiceResponseParamsTest, IsValidSigninMultipleAccountsInitiatorNotSet) {
  DiceResponseParams params;
  DiceResponseParams::SigninInfo* signin_info =
      &params.data.emplace<DiceResponseParams::SigninInfo>();

  signin_info->AddAccount(
      {{GaiaId("id"), "email", 0}, "code", false, "binding", false});
  EXPECT_TRUE(params.IsValid());
  signin_info->AddAccount(
      {{GaiaId("id2"), "email2", 0}, "code2", false, "binding2", false});
  EXPECT_EQ(signin_info->GetInitiator(), nullptr);
  EXPECT_FALSE(params.IsValid());

  // Empty Gaia id.
  signin_info->set_linked_accounts_metadata(
      DiceResponseParams::SigninInfo::LinkedAccountsMetadata{
          .primary_is_connected = Tribool::kTrue, .initiator_id = GaiaId()});
  EXPECT_FALSE(params.IsValid());

  signin_info->set_linked_accounts_metadata(
      DiceResponseParams::SigninInfo::LinkedAccountsMetadata{
          .primary_is_connected = Tribool::kTrue,
          .initiator_id = GaiaId("id")});
  EXPECT_TRUE(params.IsValid());
}

TEST(SigninInfoTest, SigninInfoGetInitiator) {
  DiceResponseParams::SigninInfo signin_info;

  // No accounts: returns nullptr.
  EXPECT_EQ(nullptr, signin_info.GetInitiator());

  DiceResponseParams::SigninInfo::SigninAccount account1;
  account1.account_info.gaia_id = GaiaId("id1");
  signin_info.AddAccount(account1);

  // One account, no initiator: returns the account.
  EXPECT_EQ(&signin_info.accounts()[0], signin_info.GetInitiator());

  DiceResponseParams::SigninInfo::SigninAccount account2;
  account2.account_info.gaia_id = GaiaId("id2");
  signin_info.AddAccount(account2);

  // Multiple accounts, no initiator: returns nullptr.
  EXPECT_EQ(nullptr, signin_info.GetInitiator());

  // Set initiator to account 2.
  signin_info.set_linked_accounts_metadata(
      DiceResponseParams::SigninInfo::LinkedAccountsMetadata{
          .primary_is_connected = Tribool::kTrue,
          .initiator_id = GaiaId("id2")});
  EXPECT_EQ(&signin_info.accounts()[1], signin_info.GetInitiator());

  // Set initiator to account 1.
  signin_info.set_linked_accounts_metadata(
      DiceResponseParams::SigninInfo::LinkedAccountsMetadata{
          .primary_is_connected = Tribool::kTrue,
          .initiator_id = GaiaId("id1")});
  EXPECT_EQ(&signin_info.accounts()[0], signin_info.GetInitiator());

  // Set initiator to unknown account: returns nullptr.
  signin_info.set_linked_accounts_metadata(
      DiceResponseParams::SigninInfo::LinkedAccountsMetadata{
          .primary_is_connected = Tribool::kTrue,
          .initiator_id = GaiaId("unknown")});
  EXPECT_EQ(nullptr, signin_info.GetInitiator());
}

TEST(DiceResponseParamsTest, IsValidSignout) {
  DiceResponseParams params;
  DiceResponseParams::SignoutInfo* signout_info =
      &params.data.emplace<DiceResponseParams::SignoutInfo>();
  EXPECT_FALSE(params.IsValid());  // Empty account_infos.

  DiceResponseParams::AccountInfo info;
  signout_info->account_infos.push_back(info);
  EXPECT_TRUE(params.IsValid());
}

TEST(DiceResponseParamsTest, IsValidEnableSync) {
  DiceResponseParams params;
  DiceResponseParams::EnableSyncInfo* enable_sync_info =
      &params.data.emplace<DiceResponseParams::EnableSyncInfo>();
  EXPECT_FALSE(params.IsValid());  // Invalid account info.

  enable_sync_info->account_info.gaia_id = GaiaId("id");
  enable_sync_info->account_info.email = "email";
  enable_sync_info->account_info.session_index = 0;
  EXPECT_TRUE(params.IsValid());
}

}  // namespace signin
