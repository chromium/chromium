// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/db/allowlist_checker_client.h"

#include <memory>

#include "base/bind.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {

// Number of milliseconds to wait for the response from the Safe Browsing
// database manager before proceeding with the timeout behavior.
const int kLookupTimeoutMS = 5000;
}  // namespace

// static
void AllowlistCheckerClient::StartCheckCsdWhitelist(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    const GURL& url,
    base::Callback<void(bool)> callback_for_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // On timeout or if the list is unavailable, report match.
  const bool kDefaultDoesMatchAllowlist = true;

  std::unique_ptr<AllowlistCheckerClient> client = GetAllowlistCheckerClient(
      database_manager, url, callback_for_result, kDefaultDoesMatchAllowlist);
  if (!client) {
    callback_for_result.Run(kDefaultDoesMatchAllowlist);
    return;
  }

  AsyncMatch match = database_manager->CheckCsdWhitelistUrl(url, client.get());
  InvokeCallbackOrRelease(match, std::move(client));
}

// static
void AllowlistCheckerClient::StartCheckHighConfidenceAllowlist(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    const GURL& url,
    base::Callback<void(bool)> callback_for_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // On timeout or if the list is unavailable, report no match.
  const bool kDefaultDoesMatchAllowlist = false;

  std::unique_ptr<AllowlistCheckerClient> client = GetAllowlistCheckerClient(
      database_manager, url, callback_for_result, kDefaultDoesMatchAllowlist);
  if (!client) {
    callback_for_result.Run(kDefaultDoesMatchAllowlist);
    return;
  }

  AsyncMatch match =
      database_manager->CheckUrlForHighConfidenceAllowlist(url, client.get());
  InvokeCallbackOrRelease(match, std::move(client));
}

// static
std::unique_ptr<AllowlistCheckerClient>
AllowlistCheckerClient::GetAllowlistCheckerClient(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    const GURL& url,
    base::Callback<void(bool)> callback_for_result,
    bool default_does_match_allowlist) {
  if (!url.is_valid() || !database_manager ||
      !database_manager->CanCheckUrl(url)) {
    return nullptr;
  }

  // Make a client for each request. The caller could have several in
  // flight at once.
  return std::make_unique<AllowlistCheckerClient>(
      callback_for_result, database_manager, default_does_match_allowlist);
}

// static
void AllowlistCheckerClient::InvokeCallbackOrRelease(
    AsyncMatch match,
    std::unique_ptr<AllowlistCheckerClient> client) {
  switch (match) {
    case AsyncMatch::MATCH:
      client->callback_for_result_.Run(true /* did_match_allowlist */);
      break;
    case AsyncMatch::NO_MATCH:
      client->callback_for_result_.Run(false /* did_match_allowlist */);
      break;
    case AsyncMatch::ASYNC:
      // Client is now self-owned. When it gets called back with the result,
      // it will delete itself.
      client.release();
      break;
  }
}

AllowlistCheckerClient::AllowlistCheckerClient(
    base::Callback<void(bool)> callback_for_result,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    bool default_does_match_allowlist)
    : callback_for_result_(callback_for_result),
      database_manager_(database_manager),
      default_does_match_allowlist_(default_does_match_allowlist) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Set a timer to fail open, i.e. call it "whitelisted", if the full
  // check takes too long.
  auto timeout_callback = base::Bind(&AllowlistCheckerClient::OnTimeout,
                                     weak_factory_.GetWeakPtr());
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kLookupTimeoutMS),
               timeout_callback);
}

AllowlistCheckerClient::~AllowlistCheckerClient() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

// SafeBrowsingDatabaseMananger::Client impl
void AllowlistCheckerClient::OnCheckWhitelistUrlResult(
    bool did_match_allowlist) {
  OnCheckUrlResult(did_match_allowlist);
}

void AllowlistCheckerClient::OnCheckUrlForHighConfidenceAllowlist(
    bool did_match_allowlist) {
  OnCheckUrlResult(did_match_allowlist);
}

void AllowlistCheckerClient::OnCheckUrlResult(bool did_match_allowlist) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  timer_.Stop();
  callback_for_result_.Run(did_match_allowlist);

  // This method is invoked only if we're already self-owned.
  delete this;
}

void AllowlistCheckerClient::OnTimeout() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  database_manager_->CancelCheck(this);
  OnCheckUrlResult(default_does_match_allowlist_);
}

}  // namespace safe_browsing
