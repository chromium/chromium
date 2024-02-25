// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/history_delegate_impl.h"

#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/segmentation_platform/internal/signals/url_signal_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {

using ::testing::_;
using ::testing::Return;

base::CancelableTaskTracker::TaskId RunNotFoundCallback(
    const GURL& url,
    bool want_visits,
    history::HistoryService::QueryURLCallback callback,
    base::CancelableTaskTracker* tracker) {
  history::QueryURLResult result;
  result.success = false;
  std::move(callback).Run(result);
  return base::CancelableTaskTracker::TaskId();
}

base::CancelableTaskTracker::TaskId RunFoundCallback(
    const GURL& url,
    bool want_visits,
    history::HistoryService::QueryURLCallback callback,
    base::CancelableTaskTracker* tracker) {
  history::QueryURLResult result;
  result.success = true;
  result.row.set_url(url);
  result.row.set_last_visit(base::Time::Now());
  std::move(callback).Run(result);
  return base::CancelableTaskTracker::TaskId();
}

class MockHistoryService : public history::HistoryService {
 public:
  MOCK_METHOD4(QueryURL,
               base::CancelableTaskTracker::TaskId(
                   const GURL& url,
                   bool want_visits,
                   QueryURLCallback callback,
                   base::CancelableTaskTracker* tracker));
};

}  // namespace

class HistoryDelegateImplTest : public testing::Test {
 public:
  HistoryDelegateImplTest() = default;
  ~HistoryDelegateImplTest() override = default;

  void SetUp() override {
    signal_handler_ = std::make_unique<UrlSignalHandler>(nullptr);
    history_delegate_ = std::make_unique<HistoryDelegateImpl>(
        &history_service_, signal_handler_.get(), /*profile_id*/ "test_id");
  }

  void TearDown() override {
    history_delegate_.reset();
    signal_handler_.reset();
  }

  HistoryDelegateImpl& history_delegate() { return *history_delegate_; }
  MockHistoryService& history_service() { return history_service_; }

 private:
  MockHistoryService history_service_;
  std::unique_ptr<UrlSignalHandler> signal_handler_;
  std::unique_ptr<HistoryDelegateImpl> history_delegate_;
};

TEST_F(HistoryDelegateImplTest, FindInHistory) {
  const GURL kUrl1("https://www.url1.com");
  EXPECT_CALL(history_service(), QueryURL(kUrl1, false, _, _))
      .WillOnce(&RunNotFoundCallback);
  history_delegate().FindUrlInHistory(
      kUrl1, base::BindOnce([](bool found, const std::string& profile_id) {
        EXPECT_FALSE(found);
        EXPECT_EQ(profile_id, "");
      }));

  EXPECT_CALL(history_service(), QueryURL(kUrl1, false, _, _))
      .WillOnce(&RunFoundCallback);
  history_delegate().FindUrlInHistory(
      kUrl1, base::BindOnce([](bool found, const std::string& profile_id) {
        EXPECT_TRUE(found);
        EXPECT_EQ(profile_id, "test_id");
      }));
}

TEST_F(HistoryDelegateImplTest, FastCheck) {
  const GURL kUrl1("https://www.url1.com");
  const GURL kUrl2("https://www.url2.com");
  EXPECT_FALSE(history_delegate().FastCheckUrl(kUrl1));
  EXPECT_FALSE(history_delegate().FastCheckUrl(kUrl2));

  history_delegate().OnUrlAdded(kUrl1);
  EXPECT_TRUE(history_delegate().FastCheckUrl(kUrl1));
  EXPECT_FALSE(history_delegate().FastCheckUrl(kUrl2));

  history_delegate().OnUrlAdded(kUrl2);
  history_delegate().OnUrlAdded(kUrl2);
  EXPECT_TRUE(history_delegate().FastCheckUrl(kUrl1));
  EXPECT_TRUE(history_delegate().FastCheckUrl(kUrl2));

  history_delegate().OnUrlRemoved({kUrl1, kUrl2, GURL()});
  EXPECT_FALSE(history_delegate().FastCheckUrl(kUrl1));
  EXPECT_FALSE(history_delegate().FastCheckUrl(kUrl2));
}

}  // namespace segmentation_platform
