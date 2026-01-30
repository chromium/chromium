// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_SUPERVISED_USER_URL_FILTER_TEST_UTILS_H_
#define COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_SUPERVISED_USER_URL_FILTER_TEST_UTILS_H_

#include <cstddef>
#include <deque>
#include <queue>
#include <string>

#include "base/memory/raw_ref.h"
#include "components/safe_search_api/fake_url_checker_client.h"
#include "components/safe_search_api/url_checker_client.h"
#include "components/supervised_user/core/browser/family_link_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace version_info {
enum class Channel;
}

namespace supervised_user {

class FakeURLFilterDelegate : public FamilyLinkUrlFilter::Delegate {
 public:
  bool SupportsWebstoreURL(const GURL& url) const override;
};

class FakePlatformDelegate : public SupervisedUserService::PlatformDelegate {
 public:
  std::string GetCountryCode() const override;
  version_info::Channel GetChannel() const override;
  bool ShouldCloseIncognitoTabs() const override;
  void CloseIncognitoTabs() override;
};

// Quite similar to safe_search_api::FakeURLCheckerClient, but offers a
// deque-style interface for the callback resolution - which is useful for
// testing scenarios with parallel url checks; and sync-style resolution of
// the callbacks.
class MockUrlCheckerClient : public safe_search_api::URLCheckerClient {
 public:
  MockUrlCheckerClient();
  MockUrlCheckerClient(const MockUrlCheckerClient&) = delete;
  MockUrlCheckerClient& operator=(const MockUrlCheckerClient&) = delete;
  ~MockUrlCheckerClient() override;

  MOCK_METHOD(void,
              CheckURL,
              (const GURL& url, ClientCheckCallback callback),
              (override));

  void RunFirstCallack(safe_search_api::ClientClassification classification);
  void RunLastCallack(safe_search_api::ClientClassification classification);

  // Next CheckURL will use scheduled resolutions synchronously, until
  // exhausted.
  void ScheduleResolution(safe_search_api::ClientClassification classification);

  std::size_t GetPendingChecksCount() const;

 private:
  struct PendingCheck {
    GURL url;
    ClientCheckCallback callback;

    PendingCheck(const GURL& url, ClientCheckCallback callback);
    PendingCheck(const PendingCheck& other) = delete;
    PendingCheck& operator=(const PendingCheck& other) = delete;
    ~PendingCheck();
  };
  std::deque<PendingCheck> pending_checks_;
  std::queue<safe_search_api::ClientClassification> resolutions_;
};

// Wrapper around any Url checker client. It allows multiplexing the checker
// client so it's both injected into the various supervised user components and
// accessible to the test fixture.
class UrlCheckerClientWrapper : public safe_search_api::URLCheckerClient {
 public:
  explicit UrlCheckerClientWrapper(safe_search_api::URLCheckerClient& client);
  UrlCheckerClientWrapper(const UrlCheckerClientWrapper&) = delete;
  UrlCheckerClientWrapper& operator=(const UrlCheckerClientWrapper&) = delete;
  ~UrlCheckerClientWrapper() override;

  void CheckURL(const GURL& url, ClientCheckCallback callback) override;

 private:
  // Actual client.
  raw_ref<safe_search_api::URLCheckerClient> client_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_SUPERVISED_USER_URL_FILTER_TEST_UTILS_H_
