// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/controller/browser_dm_token_storage.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "build/build_config.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/policy/core/common/policy_logger.h"

namespace policy {

namespace {

constexpr char kInvalidTokenValue[] = "INVALID_DM_TOKEN";

DMToken CreateValidToken(const std::string& dm_token) {
  DCHECK_NE(dm_token, kInvalidTokenValue);
  DCHECK(!dm_token.empty());
  return DMToken::CreateValidToken(dm_token);
}

DMToken CreateInvalidToken() {
  return DMToken::CreateInvalidToken();
}

DMToken CreateEmptyToken() {
  return DMToken::CreateEmptyToken();
}

}  // namespace

// static
BrowserDMTokenStorage* BrowserDMTokenStorage::storage_for_testing_ = nullptr;

BrowserDMTokenStorage* BrowserDMTokenStorage::Get() {
  if (storage_for_testing_)
    return storage_for_testing_;

  static base::NoDestructor<BrowserDMTokenStorage> storage;
  return storage.get();
}

// static
void BrowserDMTokenStorage::SetDelegate(std::unique_ptr<Delegate> delegate) {
  auto* storage = BrowserDMTokenStorage::Get();

  if (!delegate || storage->delegate_) {
    return;
  }

  BrowserDMTokenStorage::Get()->delegate_ = std::move(delegate);
}

BrowserDMTokenStorage::BrowserDMTokenStorage() : dm_token_(CreateEmptyToken()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  // We don't call InitIfNeeded() here so that the global instance can be
  // created early during startup if needed. The tokens and client ID are read
  // from the system as part of the first retrieve or store operation.
}

BrowserDMTokenStorage::~BrowserDMTokenStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::string BrowserDMTokenStorage::RetrieveClientId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  InitIfNeeded();
  return client_id_;
}

std::string BrowserDMTokenStorage::RetrieveEnrollmentToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  InitIfNeeded();
  return enrollment_token_;
}

void BrowserDMTokenStorage::StoreDMToken(const std::string& dm_token,
                                         StoreCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!store_callback_);
  InitIfNeeded();

  store_callback_ = std::move(callback);

  if (dm_token.empty()) {
    dm_token_ = CreateEmptyToken();
    DeleteDMToken();
  } else if (dm_token == kInvalidTokenValue) {
    dm_token_ = CreateInvalidToken();
    SaveDMToken(kInvalidTokenValue);
  } else {
    dm_token_ = CreateValidToken(dm_token);
    SaveDMToken(dm_token_.value());
  }
}

void BrowserDMTokenStorage::InvalidateDMToken(StoreCallback callback) {
  StoreDMToken(kInvalidTokenValue, std::move(callback));
}

void BrowserDMTokenStorage::ClearDMToken(StoreCallback callback) {
  StoreDMToken("", std::move(callback));
}

DMToken BrowserDMTokenStorage::RetrieveDMToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  InitIfNeeded();
  return dm_token_;
}

void BrowserDMTokenStorage::OnDMTokenStored(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_callback_);

  if (!store_callback_.is_null())
    std::move(store_callback_).Run(success);
}

bool BrowserDMTokenStorage::ShouldDisplayErrorMessageOnFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  InitIfNeeded();
  return should_display_error_message_on_failure_;
}

void BrowserDMTokenStorage::InitIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_) << "DM storage delegate has not been set. If this is a "
                       "test, you may need to add an instance of "
                       "FakeBrowserDMTokenStorage to the test fixture.";

  if (is_initialized_) {
    // TODO(crbug.com/40893625): Ideally we would execute this initialization
    // based on an event we listen to. However, because this may happen so
    // early, we don't have any place where we can hook this. We should find
    // a better solution in the future.
    if (is_init_enrollment_token_skipped_) {
      is_init_enrollment_token_skipped_ = !delegate_->CanInitEnrollmentToken();
      enrollment_token_ = delegate_->InitEnrollmentToken();
    }
    return;
  }

  is_initialized_ = true;

  // The enrollment token initialization may not be possible on the first call
  // to `InitIfNeeded` on all platforms. `CanInitEnrollmentToken` will return
  // false if this was the case to try initializing the token on the next call
  // to `InitIfNeeded` and avoid returning an empty token when
  // `RetrieveEnnrollmentToken' is called. It returns true on platforms that do
  // not have this problem.
  is_init_enrollment_token_skipped_ = !delegate_->CanInitEnrollmentToken();

  // When CBCM is not enabled, set the DM token to empty directly withtout
  // actually read it.
  if (!ChromeBrowserCloudManagementController::IsEnabled()) {
    dm_token_ = CreateEmptyToken();
    return;
  }

  // Only supported in official builds.
  client_id_ = delegate_->InitClientId();
  DVLOG(1) << "Client ID = " << client_id_;
  if (client_id_.empty())
    return;

  // checks if client ID is greater than 64 characters
  if (client_id_.length() > 64) {
    SYSLOG(ERROR) << "Chrome browser cloud management client ID should"
                     "not be greater than 64 characters long.";
    client_id_.clear();
    return;
  }

  // checks if client ID includes an illegal character
  if (base::ranges::any_of(client_id_, [](char ch) {
        return ch == ' ' || !base::IsAsciiPrintable(ch);
      })) {
    SYSLOG(ERROR)
        << "Chrome browser cloud management client ID should not"
           " contain a space, new line, or any nonprintable character.";
    client_id_.clear();
    return;
  }

  enrollment_token_ = delegate_->InitEnrollmentToken();
  DVLOG(1) << "Enrollment token = " << enrollment_token_;

  std::string init_dm_token = delegate_->InitDMToken();
  if (init_dm_token.empty()) {
    dm_token_ = CreateEmptyToken();
    DVLOG(1) << "DM Token = empty";
  } else if (init_dm_token == kInvalidTokenValue) {
    dm_token_ = CreateInvalidToken();
    DVLOG(1) << "DM Token = invalid";
  } else {
    dm_token_ = CreateValidToken(init_dm_token);
    DVLOG(1) << "DM Token = " << dm_token_.value();
  }

  should_display_error_message_on_failure_ =
      delegate_->InitEnrollmentErrorOption();
}

void BrowserDMTokenStorage::SaveDMToken(const std::string& token) {
  auto task = delegate_->SaveDMTokenTask(token, RetrieveClientId());
  auto reply = base::BindOnce(&BrowserDMTokenStorage::OnDMTokenStored,
                              weak_factory_.GetWeakPtr());
  delegate_->SaveDMTokenTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(task), std::move(reply));
}

void BrowserDMTokenStorage::DeleteDMToken() {
  auto task = delegate_->DeleteDMTokenTask(RetrieveClientId());
  auto reply = base::BindOnce(&BrowserDMTokenStorage::OnDMTokenStored,
                              weak_factory_.GetWeakPtr());
  delegate_->SaveDMTokenTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(task), std::move(reply));
}

}  // namespace policy
