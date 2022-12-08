// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/screentime/history_bridge.h"

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/cocoa/screentime/history_deleter.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace screentime {

namespace {

class TestHistoryDeleter : public HistoryDeleter {
 public:
  ~TestHistoryDeleter() override {}

  bool deleted_all() const { return deleted_all_; }
  absl::optional<TimeInterval> deleted_interval() const {
    return deleted_interval_;
  }
  const std::set<GURL>& deleted_urls() const { return deleted_urls_; }

  void WaitForDelete() { wait_loop_.Run(); }

  // HistoryDeleter:
  void DeleteAllHistory() override {
    deleted_all_ = true;
    wait_loop_.Quit();
  }
  void DeleteHistoryDuringInterval(const TimeInterval& interval) override {
    deleted_interval_ = interval;
    wait_loop_.Quit();
  }
  void DeleteHistoryForURL(const GURL& url) override {
    deleted_urls_.insert(url);
    wait_loop_.Quit();
  }

 private:
  bool deleted_all_ = false;
  absl::optional<TimeInterval> deleted_interval_ = absl::nullopt;
  std::set<GURL> deleted_urls_;
  base::RunLoop wait_loop_;
};

}  // namespace

class HistoryBridgeTest : public ::testing::Test {
 public:
  HistoryBridgeTest() {
    service_ = std::make_unique<history::HistoryService>();
    auto deleter = std::make_unique<TestHistoryDeleter>();
    deleter_ = deleter.get();
    bridge_ =
        std::make_unique<HistoryBridge>(service_.get(), std::move(deleter));

    CHECK(history_dir_.CreateUniqueTempDir());
    service_->Init(history::HistoryDatabaseParams(
        history_dir_.GetPath(), 0, 0, version_info::Channel::UNKNOWN));
    service_->SetOnBackendDestroyTask(history_teardown_loop_.QuitClosure());
  }

  void TearDown() override {
    service()->Shutdown();
    history_teardown_loop_.Run();
  }

  history::HistoryService* service() { return service_.get(); }
  TestHistoryDeleter* deleter() { return deleter_; }

  void AddPage(const GURL& url, base::Time time = base::Time::Now()) {
    service()->AddPage(url, time, history::VisitSource::SOURCE_BROWSED);
  }

  void DeleteHistoryBetween(base::Time start, base::Time end) {
    base::CancelableTaskTracker tracker;
    base::RunLoop loop;
    service()->ExpireHistoryBetween({}, start, end, true, loop.QuitClosure(),
                                    &tracker);
    loop.Run();
  }

  void DeleteHistoryForURL(const GURL& url) { service()->DeleteURLs({url}); }

  void DeleteAllHistory() { DeleteHistoryBetween(base::Time(), base::Time()); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> service_;
  raw_ptr<TestHistoryDeleter> deleter_;
  std::unique_ptr<HistoryBridge> bridge_;
  base::RunLoop history_teardown_loop_;
};

TEST_F(HistoryBridgeTest, DeleteAll) {
  AddPage(GURL("https://www.chromium.org/"));
  AddPage(GURL("https://test.chromium.org/"));

  DeleteAllHistory();
  deleter()->WaitForDelete();
  EXPECT_TRUE(deleter()->deleted_all());
}

TEST_F(HistoryBridgeTest, DeleteURLs) {
  const GURL kTestUrlA("https://www.chromium.org/");
  const base::Time now = base::Time::Now();
  AddPage(kTestUrlA, now - base::Seconds(2));
  AddPage(GURL("https://test.chromium.org/"), now - base::Seconds(1));

  service()->DeleteURLs({kTestUrlA});
  deleter()->WaitForDelete();
  EXPECT_FALSE(deleter()->deleted_all());
  EXPECT_EQ(deleter()->deleted_urls(), std::set<GURL>{kTestUrlA});
}

TEST_F(HistoryBridgeTest, DeleteTimeInterval) {
  const base::Time now = base::Time::Now();
  AddPage(GURL("https://www.chromium.org/a"), now - base::Seconds(2));
  AddPage(GURL("https://www.chromium.org/b"), now - base::Seconds(1));

  DeleteHistoryBetween(now - base::Seconds(3), now);
  deleter()->WaitForDelete();
  EXPECT_FALSE(deleter()->deleted_all());
  EXPECT_EQ(deleter()->deleted_interval()->first, now - base::Seconds(3));
  EXPECT_EQ(deleter()->deleted_interval()->second, now);
}

TEST_F(HistoryBridgeTest, OnlyOriginsAreDeleted) {
  const GURL kTestURL("https://www.chromium.org/abc");
  const GURL kStrippedTestURL("https://www.chromium.org/");
  AddPage(kTestURL);
  DeleteHistoryForURL(kTestURL);
  deleter()->WaitForDelete();
  EXPECT_EQ(deleter()->deleted_urls().size(), 1U);
  EXPECT_EQ(deleter()->deleted_urls().count(kStrippedTestURL), 1U);
}

}  // namespace screentime
