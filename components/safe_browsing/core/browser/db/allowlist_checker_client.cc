// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/allowlist_checker_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"

namespace safe_browsing {

namespace {

// Number of milliseconds to wait for the response from the Safe Browsing
// database manager before proceeding with the timeout behavior.
const int kLookupTimeoutMS = 5000;
}  // namespace

// static
void AllowlistCheckerClient::StartCheckCsdAllowlist(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    const GURL& url,
    base::OnceCallback<void(bool)> callback_for_result) {
  // On timeout or if the list is unavailable, report match.
  const bool kDefaultDoesMatchAllowlist = true;

  std::unique_ptr<AllowlistCheckerClient> client = GetAllowlistCheckerClient(
      database_manager, url, &callback_for_result, kDefaultDoesMatchAllowlist);
  if (!client) {
    std::move(callback_for_result).Run(kDefaultDoesMatchAllowlist);
    return;
  }

  AsyncMatch match = database_manager->CheckCsdAllowlistUrl(url, client.get());
  base::UmaHistogramEnumeration(
      "SafeBrowsing.ClientSidePhishingDetection.AllowlistMatchResult", match);
  InvokeCallbackOrRelease(match, std::move(client));
}

// static
std::unique_ptr<AllowlistCheckerClient>
AllowlistCheckerClient::GetAllowlistCheckerClient(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    const GURL& url,
    base::OnceCallback<void(bool)>* callback_for_result,
    bool default_does_match_allowlist) {
  if (!url.is_valid() || !database_manager ||
      !database_manager->CanCheckUrl(url)) {
    return nullptr;
  }

  // Make a client for each request. The caller could have several in
  // flight at once.
  return std::make_unique<AllowlistCheckerClient>(
      std::move(*callback_for_result), database_manager,
      default_does_match_allowlist);
}

// static
void AllowlistCheckerClient::InvokeCallbackOrRelease(
    AsyncMatch match,
    std::unique_ptr<AllowlistCheckerClient> client) {
  switch (match) {
    case AsyncMatch::MATCH:
      std::move(client->callback_for_result_)
          .Run(true /* did_match_allowlist */);
      break;
    case AsyncMatch::NO_MATCH:
      std::move(client->callback_for_result_)
          .Run(false /* did_match_allowlist */);
      break;
    case AsyncMatch::ASYNC:
      // Client is now self-owned. When it gets called back with the result,
      // it will delete itself.
      client.release();
      break;
  }
}

AllowlistCheckerClient::AllowlistCheckerClient(
    base::OnceCallback<void(bool)> callback_for_result,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    bool default_does_match_allowlist)
    : callback_for_result_(std::move(callback_for_result)),
      database_manager_(database_manager),
      default_does_match_allowlist_(default_does_match_allowlist) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Set a timer to fail open, i.e. call it "allowlisted", if the full
  // check takes too long.
  auto timeout_callback = base::BindOnce(&AllowlistCheckerClient::OnTimeout,
                                         weak_factory_.GetWeakPtr());
  timer_.Start(FROM_HERE, base::Milliseconds(kLookupTimeoutMS),
               std::move(timeout_callback));
}

AllowlistCheckerClient::~AllowlistCheckerClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// SafeBrowsingDatabaseMananger::Client impl
void AllowlistCheckerClient::OnCheckAllowlistUrlResult(
    bool did_match_allowlist) {
  OnCheckUrlResult(did_match_allowlist);
}

void AllowlistCheckerClient::OnCheckUrlResult(bool did_match_allowlist) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Stop();

  // The callback can only be invoked by other code paths if this object is not
  // self-owned. Because this method is only invoked when we're self-owned, we
  // know the callback must still be valid, and it must be safe to delete
  // |this|.
  DCHECK(callback_for_result_);
  std::move(callback_for_result_).Run(did_match_allowlist);
  delete this;
}

void AllowlistCheckerClient::OnTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  database_manager_->CancelCheck(this);
  OnCheckUrlResult(default_does_match_allowlist_);
}

}  // namespace safe_browsing
