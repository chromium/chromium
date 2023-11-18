// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/url_signal_handler.h"

#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/mock_ukm_database.h"
#include "components/segmentation_platform/internal/database/ukm_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::ReturnRef;

void RunFoundCallback(const GURL& url,
                      UrlSignalHandler::FindCallback callback) {
  std::move(callback).Run(true, "found_callback_id");
}
void RunNotFoundCallback(const GURL& url,
                         UrlSignalHandler::FindCallback callback) {
  std::move(callback).Run(false, "");
}

class MockHistoryDelegate : public UrlSignalHandler::HistoryDelegate {
 public:
  MOCK_METHOD(bool, FastCheckUrl, (const GURL& url), (override));

  MOCK_METHOD(void,
              FindUrlInHistory,
              (const GURL& url, UrlSignalHandler::FindCallback callback),
              (override));

  MOCK_METHOD(const std::string&, profile_id, (), (override));
};

class UrlSignalHandlerTest : public testing::Test {
 public:
  UrlSignalHandlerTest() = default;
  ~UrlSignalHandlerTest() override = default;

  void SetUp() override {
    signal_handler_ = std::make_unique<UrlSignalHandler>(&ukm_database_);
  }

  void TearDown() override { signal_handler_.reset(); }

  UrlSignalHandler& signal_handler() { return *signal_handler_; }

  MockUkmDatabase& ukm_database() { return ukm_database_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockUkmDatabase ukm_database_;
  std::unique_ptr<UrlSignalHandler> signal_handler_;
};

TEST_F(UrlSignalHandlerTest, TestNoHistoryDelegate) {
  const GURL kUrl1("https://www.url1.com");
  const ukm::SourceId kSourceId1 = 10;

  EXPECT_CALL(ukm_database(),
              UpdateUrlForUkmSource(kSourceId1, kUrl1, false, ""));
  signal_handler().OnUkmSourceUpdated(kSourceId1, {kUrl1});
}

TEST_F(UrlSignalHandlerTest, TestFastCheckSuccess) {
  const GURL kUrl1("https://www.url1.com");
  const ukm::SourceId kSourceId1 = 10;
  MockHistoryDelegate history_delegate;
  const std::string profile_id = "test_id";

  EXPECT_CALL(history_delegate, profile_id())
      .WillRepeatedly(ReturnRef(profile_id));
  EXPECT_CALL(history_delegate, FastCheckUrl(kUrl1)).WillOnce(Return(true));
  EXPECT_CALL(history_delegate, FindUrlInHistory(_, _)).Times(0);
  EXPECT_CALL(ukm_database(),
              UpdateUrlForUkmSource(kSourceId1, kUrl1, true, profile_id));

  signal_handler().AddHistoryDelegate(&history_delegate);
  signal_handler().OnUkmSourceUpdated(kSourceId1, {kUrl1});
  signal_handler().RemoveHistoryDelegate(&history_delegate);
}

TEST_F(UrlSignalHandlerTest, TestMultipleDelegates) {
  const GURL kUrl1("https://www.url1.com");
  const GURL kUrl2("https://www.url2.com");
  const ukm::SourceId kSourceId1 = 10;
  const ukm::SourceId kSourceId2 = 10;
  MockHistoryDelegate history_delegate1;
  MockHistoryDelegate history_delegate2;
  const std::string profile_id = "test_id";

  // URL1 is not found in both the history delegates.
  EXPECT_CALL(history_delegate2, profile_id())
      .WillRepeatedly(ReturnRef(profile_id));
  EXPECT_CALL(history_delegate1, FastCheckUrl(kUrl1)).WillOnce(Return(false));
  EXPECT_CALL(history_delegate2, FastCheckUrl(kUrl1)).WillOnce(Return(false));
  EXPECT_CALL(history_delegate1, FindUrlInHistory(kUrl1, _))
      .WillOnce(&RunNotFoundCallback);
  EXPECT_CALL(history_delegate2, FindUrlInHistory(kUrl1, _))
      .WillOnce(&RunNotFoundCallback);
  // URL2 is found in fast search of second delegate. Delegate1 may or may not
  // be called depending on which delegate is checked first.
  EXPECT_CALL(history_delegate1, FastCheckUrl(kUrl2))
      .Times(AnyNumber())
      .WillOnce(Return(false));
  EXPECT_CALL(history_delegate2, FastCheckUrl(kUrl2)).WillOnce(Return(true));

  EXPECT_CALL(ukm_database(),
              UpdateUrlForUkmSource(kSourceId1, kUrl1, false, ""));
  EXPECT_CALL(ukm_database(),
              UpdateUrlForUkmSource(kSourceId1, kUrl2, true, profile_id));

  signal_handler().AddHistoryDelegate(&history_delegate1);
  signal_handler().AddHistoryDelegate(&history_delegate2);
  signal_handler().OnUkmSourceUpdated(kSourceId1, {kUrl1});
  signal_handler().OnUkmSourceUpdated(kSourceId2, {kUrl2});
  signal_handler().RemoveHistoryDelegate(&history_delegate1);
  signal_handler().RemoveHistoryDelegate(&history_delegate2);
}

TEST_F(UrlSignalHandlerTest, FindInHistory) {
  const GURL kUrl1("https://www.url1.com");
  const GURL kUrl2("https://www.url2.com");
  const ukm::SourceId kSourceId1 = 10;
  const ukm::SourceId kSourceId2 = 10;
  MockHistoryDelegate history_delegate1;
  MockHistoryDelegate history_delegate2;

  // URL1 is found in history search of first delegate. Delegate2 may or may not
  // be called depending on which delegate is checked first.
  EXPECT_CALL(history_delegate1, FastCheckUrl(kUrl1)).WillOnce(Return(false));
  EXPECT_CALL(history_delegate2, FastCheckUrl(kUrl1)).WillOnce(Return(false));
  EXPECT_CALL(history_delegate1, FindUrlInHistory(kUrl1, _))
      .WillOnce(&RunFoundCallback);
  EXPECT_CALL(history_delegate2, FindUrlInHistory(kUrl1, _))
      .Times(AnyNumber())
      .WillOnce(&RunNotFoundCallback);
  // URL1 is not found in both the history delegates.
  EXPECT_CALL(history_delegate1, FastCheckUrl(kUrl2)).WillOnce(Return(false));
  EXPECT_CALL(history_delegate2, FastCheckUrl(kUrl2)).WillOnce(Return(false));
  EXPECT_CALL(history_delegate1, FindUrlInHistory(kUrl2, _))
      .WillOnce(&RunNotFoundCallback);
  EXPECT_CALL(history_delegate2, FindUrlInHistory(kUrl2, _))
      .WillOnce(&RunNotFoundCallback);

  EXPECT_CALL(ukm_database(), UpdateUrlForUkmSource(kSourceId1, kUrl1, true,
                                                    "found_callback_id"));
  EXPECT_CALL(ukm_database(),
              UpdateUrlForUkmSource(kSourceId1, kUrl2, false, ""));

  signal_handler().AddHistoryDelegate(&history_delegate1);
  signal_handler().AddHistoryDelegate(&history_delegate2);
  signal_handler().OnUkmSourceUpdated(kSourceId1, {kUrl1});
  signal_handler().OnUkmSourceUpdated(kSourceId2, {kUrl2});
  signal_handler().RemoveHistoryDelegate(&history_delegate1);
  signal_handler().RemoveHistoryDelegate(&history_delegate2);
}

TEST_F(UrlSignalHandlerTest, Observation) {
  const GURL kUrl1("https://www.url1.com");
  const GURL kUrl2("https://www.url2.com");

  EXPECT_CALL(ukm_database(), OnUrlValidated(kUrl1, ""));
  signal_handler().OnHistoryVisit(kUrl1, "");
  EXPECT_CALL(ukm_database(), OnUrlValidated(kUrl2, ""));
  signal_handler().OnHistoryVisit(kUrl2, "");
  std::vector<GURL> list({kUrl1, kUrl2});
  EXPECT_CALL(ukm_database(), RemoveUrls(list, false));
  signal_handler().OnUrlsRemovedFromHistory(list, false);
}

}  // namespace segmentation_platform
