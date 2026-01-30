// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"

#include <cstddef>
#include <deque>
#include <string>
#include <utility>

#include "base/test/bind.h"
#include "base/version_info/channel.h"
#include "components/safe_search_api/fake_url_checker_client.h"
#include "components/safe_search_api/url_checker_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace supervised_user {

bool FakeURLFilterDelegate::SupportsWebstoreURL(const GURL& url) const {
  return false;
}

std::string FakePlatformDelegate::GetCountryCode() const {
  // Country code information is not used in tests.
  return std::string();
}

version_info::Channel FakePlatformDelegate::GetChannel() const {
  // Channel information is not used in tests.
  return version_info::Channel::UNKNOWN;
}

// The fake should be used in supervised user context, true is a reasonable
// return default.
bool FakePlatformDelegate::ShouldCloseIncognitoTabs() const {
  return true;
}

void FakePlatformDelegate::CloseIncognitoTabs() {
  return;
}

MockUrlCheckerClient::MockUrlCheckerClient() {
  ON_CALL(*this, CheckURL)
      .WillByDefault(
          [this](
              const GURL& url,
              safe_search_api::URLCheckerClient::ClientCheckCallback callback) {
            if (!resolutions_.empty()) {
              safe_search_api::ClientClassification classification =
                  resolutions_.front();
              resolutions_.pop();
              std::move(callback).Run(url, classification);
              return;
            }
            this->pending_checks_.emplace_back(url, std::move(callback));
          });
}
MockUrlCheckerClient::~MockUrlCheckerClient() = default;

MockUrlCheckerClient::PendingCheck::PendingCheck(
    const GURL& url,
    safe_search_api::URLCheckerClient::ClientCheckCallback callback)
    : url(url), callback(std::move(callback)) {}
MockUrlCheckerClient::PendingCheck::~PendingCheck() = default;

void MockUrlCheckerClient::RunFirstCallack(
    safe_search_api::ClientClassification classification) {
  const GURL& url = pending_checks_.front().url;
  std::move(pending_checks_.front().callback).Run(url, classification);
  pending_checks_.pop_front();
}
void MockUrlCheckerClient::RunLastCallack(
    safe_search_api::ClientClassification classification) {
  const GURL& url = pending_checks_.back().url;
  std::move(pending_checks_.back().callback).Run(url, classification);
  pending_checks_.pop_back();
}
void MockUrlCheckerClient::ScheduleResolution(
    safe_search_api::ClientClassification classification) {
  resolutions_.push(classification);
}

std::size_t MockUrlCheckerClient::GetPendingChecksCount() const {
  return pending_checks_.size();
}

UrlCheckerClientWrapper::UrlCheckerClientWrapper(
    safe_search_api::URLCheckerClient& client)
    : client_(client) {}
UrlCheckerClientWrapper::~UrlCheckerClientWrapper() = default;
void UrlCheckerClientWrapper::CheckURL(
    const GURL& url,
    safe_search_api::URLCheckerClient::ClientCheckCallback callback) {
  client_->CheckURL(url, std::move(callback));
}

}  // namespace supervised_user
