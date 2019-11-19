// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/test_identity_manager_observer.h"

#include "testing/gtest/include/gtest/gtest.h"

#include <utility>

namespace signin {

TestIdentityManagerObserver::TestIdentityManagerObserver(
    IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  identity_manager_->AddObserver(this);
}

TestIdentityManagerObserver::~TestIdentityManagerObserver() {
  identity_manager_->RemoveObserver(this);
}

void TestIdentityManagerObserver::SetOnPrimaryAccountSetCallback(
    base::OnceClosure callback) {
  on_primary_account_set_callback_ = std::move(callback);
}

const CoreAccountInfo&
TestIdentityManagerObserver::PrimaryAccountFromSetCallback() {
  return primary_account_from_set_callback_;
}

void TestIdentityManagerObserver::SetOnPrimaryAccountClearedCallback(
    base::OnceClosure callback) {
  on_primary_account_cleared_callback_ = std::move(callback);
}

const CoreAccountInfo&
TestIdentityManagerObserver::PrimaryAccountFromClearedCallback() {
  return primary_account_from_cleared_callback_;
}

void TestIdentityManagerObserver::SetOnUnconsentedPrimaryAccountChangedCallback(
    base::OnceClosure callback) {
  on_unconsented_primary_account_callback_ = std::move(callback);
}

const CoreAccountInfo&
TestIdentityManagerObserver::UnconsentedPrimaryAccountFromCallback() {
  return unconsented_primary_account_from_callback_;
}

void TestIdentityManagerObserver::SetOnRefreshTokenUpdatedCallback(
    base::OnceClosure callback) {
  on_refresh_token_updated_callback_ = std::move(callback);
}

const CoreAccountInfo&
TestIdentityManagerObserver::AccountFromRefreshTokenUpdatedCallback() {
  return account_from_refresh_token_updated_callback_;
}

void TestIdentityManagerObserver::SetOnErrorStateOfRefreshTokenUpdatedCallback(
    base::OnceClosure callback) {
  on_error_state_of_refresh_token_updated_callback_ = std::move(callback);
}

const CoreAccountInfo& TestIdentityManagerObserver::
    AccountFromErrorStateOfRefreshTokenUpdatedCallback() {
  return account_from_error_state_of_refresh_token_updated_callback_;
}

const GoogleServiceAuthError&
TestIdentityManagerObserver::ErrorFromErrorStateOfRefreshTokenUpdatedCallback()
    const {
  return error_from_error_state_of_refresh_token_updated_callback_;
}

void TestIdentityManagerObserver::SetOnRefreshTokenRemovedCallback(
    base::OnceClosure callback) {
  on_refresh_token_removed_callback_ = std::move(callback);
}

const CoreAccountId&
TestIdentityManagerObserver::AccountIdFromRefreshTokenRemovedCallback() {
  return account_from_refresh_token_removed_callback_;
}

void TestIdentityManagerObserver::SetOnRefreshTokensLoadedCallback(
    base::OnceClosure callback) {
  on_refresh_tokens_loaded_callback_ = std::move(callback);
}

void TestIdentityManagerObserver::SetOnAccountsInCookieUpdatedCallback(
    base::OnceClosure callback) {
  on_accounts_in_cookie_updated_callback_ = std::move(callback);
}

const AccountsInCookieJarInfo&
TestIdentityManagerObserver::AccountsInfoFromAccountsInCookieUpdatedCallback() {
  return accounts_info_from_cookie_change_callback_;
}

const GoogleServiceAuthError&
TestIdentityManagerObserver::ErrorFromAccountsInCookieUpdatedCallback() const {
  return error_from_cookie_change_callback_;
}

void TestIdentityManagerObserver::SetOnCookieDeletedByUserCallback(
    base::OnceClosure callback) {
  on_cookie_deleted_by_user_callback_ = std::move(callback);
}

const AccountInfo&
TestIdentityManagerObserver::AccountFromAccountUpdatedCallback() {
  return account_from_account_updated_callback_;
}

const AccountInfo&
TestIdentityManagerObserver::AccountFromAccountRemovedWithInfoCallback() {
  return account_from_account_removed_with_info_callback_;
}

bool TestIdentityManagerObserver::WasCalledAccountRemovedWithInfoCallback() {
  return was_called_account_removed_with_info_callback_;
}

// Each element represents all the changes from an individual batch that has
// occurred, with the elements ordered from oldest to newest batch occurrence.
const std::vector<std::vector<CoreAccountId>>&
TestIdentityManagerObserver::BatchChangeRecords() const {
  return batch_change_records_;
}

// IdentityManager::Observer:
void TestIdentityManagerObserver::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  primary_account_from_set_callback_ = primary_account_info;
  if (on_primary_account_set_callback_)
    std::move(on_primary_account_set_callback_).Run();
}

void TestIdentityManagerObserver::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_primary_account_info) {
  primary_account_from_cleared_callback_ = previous_primary_account_info;
  if (on_primary_account_cleared_callback_)
    std::move(on_primary_account_cleared_callback_).Run();
}

void TestIdentityManagerObserver::OnUnconsentedPrimaryAccountChanged(
    const CoreAccountInfo& unconsented_primary_account_info) {
  unconsented_primary_account_from_callback_ = unconsented_primary_account_info;
  if (on_unconsented_primary_account_callback_)
    std::move(on_unconsented_primary_account_callback_).Run();
}

void TestIdentityManagerObserver::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (!is_inside_batch_)
    StartBatchOfRefreshTokenStateChanges();

  batch_change_records_.rbegin()->emplace_back(account_info.account_id);
  account_from_refresh_token_updated_callback_ = account_info;
  if (on_refresh_token_updated_callback_)
    std::move(on_refresh_token_updated_callback_).Run();
}

void TestIdentityManagerObserver::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  if (!is_inside_batch_)
    StartBatchOfRefreshTokenStateChanges();

  batch_change_records_.rbegin()->emplace_back(account_id);
  account_from_refresh_token_removed_callback_ = account_id;
  if (on_refresh_token_removed_callback_)
    std::move(on_refresh_token_removed_callback_).Run();
}

void TestIdentityManagerObserver::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error) {
  account_from_error_state_of_refresh_token_updated_callback_ = account_info;
  error_from_error_state_of_refresh_token_updated_callback_ = error;
  if (on_error_state_of_refresh_token_updated_callback_)
    std::move(on_error_state_of_refresh_token_updated_callback_).Run();
}

void TestIdentityManagerObserver::OnRefreshTokensLoaded() {
  if (on_refresh_tokens_loaded_callback_)
    std::move(on_refresh_tokens_loaded_callback_).Run();
}

void TestIdentityManagerObserver::OnAccountsInCookieUpdated(
    const AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  accounts_info_from_cookie_change_callback_ = accounts_in_cookie_jar_info;
  error_from_cookie_change_callback_ = error;
  if (on_accounts_in_cookie_updated_callback_)
    std::move(on_accounts_in_cookie_updated_callback_).Run();
}

void TestIdentityManagerObserver::OnAccountsCookieDeletedByUserAction() {
  std::move(on_cookie_deleted_by_user_callback_).Run();
}

void TestIdentityManagerObserver::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  account_from_account_updated_callback_ = info;
}

void TestIdentityManagerObserver::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  was_called_account_removed_with_info_callback_ = true;
  account_from_account_removed_with_info_callback_ = info;
}

void TestIdentityManagerObserver::StartBatchOfRefreshTokenStateChanges() {
  EXPECT_FALSE(is_inside_batch_);
  is_inside_batch_ = true;

  // Start a new batch.
  batch_change_records_.emplace_back(std::vector<CoreAccountId>());
}

void TestIdentityManagerObserver::OnEndBatchOfRefreshTokenStateChanges() {
  is_inside_batch_ = false;
}

}  // namespace signin
