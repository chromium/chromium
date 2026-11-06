// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/webui_contents_prerenderer.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using TakeResult = WebUIContentsPrerenderer::TakeResult;

constexpr base::TimeDelta kDeadline = base::Seconds(3);
constexpr base::TimeDelta kDelay = base::Milliseconds(500);

// Builds test WebContents as spares and returns a set result for each check.
class FakeDelegate : public WebUIContentsPrerenderer::Delegate {
 public:
  explicit FakeDelegate(content::BrowserContext* browser_context)
      : browser_context_(browser_context) {}

  // WebUIContentsPrerenderer::Delegate:
  std::unique_ptr<content::WebContents> CreateSpare() override {
    ++create_count;
    if (!can_create_spare) {
      return nullptr;
    }
    return content::WebContentsTester::CreateTestWebContents(
        browser_context_, content::SiteInstance::Create(browser_context_));
  }
  bool IsSpareCurrent(content::WebContents& spare) override {
    ++check_count;
    return spare_is_current;
  }

  bool can_create_spare = true;
  bool spare_is_current = true;
  int create_count = 0;
  int check_count = 0;

 private:
  raw_ptr<content::BrowserContext> browser_context_;
};

class WebUIContentsPrerendererTest : public ChromeRenderViewHostTestHarness {
 public:
  WebUIContentsPrerendererTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    delegate_ = std::make_unique<FakeDelegate>(profile());
    prerenderer_ = std::make_unique<WebUIContentsPrerenderer>(
        *delegate_,
        WebUIContentsPrerenderer::Params{
            .paint_deadline = kDeadline,
            .delay = kDelay,
            .skip_memory_limit = base::MemoryLimit::CriticalPressureThreshold(),
            .consumer_name = "WebUIContentsPrerendererTest",
        });
  }

  void TearDown() override {
    test_memory_consumer_registry_.NotifyUpdateMemoryLimit(
        base::MemoryLimit::Default());
    prerenderer_.reset();
    delegate_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<content::WebContents> CreateTestContents() {
    return content::WebContentsTester::CreateTestWebContents(
        profile(), content::SiteInstance::Create(profile()));
  }

  std::unique_ptr<content::WebContents> CreatePaintedTestContents() {
    std::unique_ptr<content::WebContents> contents = CreateTestContents();
    content::WebContentsTester::For(contents.get())
        ->TestDidFirstVisuallyNonEmptyPaint();
    return contents;
  }

  // Schedules a refill that waits for the deadline only, and runs it.
  void Refill() {
    prerenderer_->ScheduleRefill(nullptr);
    task_environment()->FastForwardBy(kDeadline + kDelay);
    ASSERT_TRUE(prerenderer_->has_spare());
  }

  FakeDelegate& delegate() { return *delegate_; }
  WebUIContentsPrerenderer& prerenderer() { return *prerenderer_; }

  base::TestMemoryConsumerRegistry test_memory_consumer_registry_;

 private:
  std::unique_ptr<FakeDelegate> delegate_;
  std::unique_ptr<WebUIContentsPrerenderer> prerenderer_;
};

TEST_F(WebUIContentsPrerendererTest, TakeWithoutSpare) {
  WebUIContentsPrerenderer::TakeOutcome outcome = prerenderer().Take();
  EXPECT_FALSE(outcome.contents);
  EXPECT_EQ(TakeResult::kNoSpare, outcome.result);
  EXPECT_EQ(0, delegate().check_count);
  // A take does not schedule a refill by itself.
  EXPECT_FALSE(prerenderer().is_refill_pending_for_testing());
}

TEST_F(WebUIContentsPrerendererTest, RefillsAfterDeadlineAndDelay) {
  prerenderer().ScheduleRefill(nullptr);
  EXPECT_TRUE(prerenderer().is_refill_pending_for_testing());

  task_environment()->FastForwardBy(kDeadline + kDelay - base::Milliseconds(1));
  EXPECT_FALSE(prerenderer().has_spare());
  task_environment()->FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(prerenderer().has_spare());
  EXPECT_FALSE(prerenderer().is_refill_pending_for_testing());
  EXPECT_EQ(1, delegate().create_count);
}

TEST_F(WebUIContentsPrerendererTest, RefillsAfterPaintBeforeStart) {
  std::unique_ptr<content::WebContents> contents = CreatePaintedTestContents();
  ASSERT_TRUE(contents->CompletedFirstVisuallyNonEmptyPaint());
  prerenderer().ScheduleRefill(contents.get());
  EXPECT_TRUE(prerenderer().is_refill_pending_for_testing());

  task_environment()->FastForwardBy(kDelay - base::Milliseconds(1));
  EXPECT_FALSE(prerenderer().has_spare());
  task_environment()->FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(prerenderer().has_spare());
  EXPECT_FALSE(prerenderer().is_refill_pending_for_testing());
  EXPECT_EQ(1, delegate().create_count);
}

TEST_F(WebUIContentsPrerendererTest, RefillsAfterPaintAfterStart) {
  std::unique_ptr<content::WebContents> contents = CreateTestContents();
  prerenderer().ScheduleRefill(contents.get());
  EXPECT_TRUE(prerenderer().is_refill_pending_for_testing());

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(prerenderer().has_spare());

  // Contents paints after 1 s.
  content::WebContentsTester::For(contents.get())
      ->TestDidFirstVisuallyNonEmptyPaint();

  task_environment()->FastForwardBy(kDelay - base::Milliseconds(1));
  EXPECT_FALSE(prerenderer().has_spare());
  task_environment()->FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(prerenderer().has_spare());
  EXPECT_FALSE(prerenderer().is_refill_pending_for_testing());
  EXPECT_EQ(1, delegate().create_count);
}

TEST_F(WebUIContentsPrerendererTest, WatchedDestroyedFallsBackToDeadline) {
  std::unique_ptr<content::WebContents> contents = CreateTestContents();
  prerenderer().ScheduleRefill(contents.get());
  EXPECT_TRUE(prerenderer().is_refill_pending_for_testing());

  // Destroy the watched WebContents before paint or deadline. The prerenderer
  // falls back to the paint deadline.
  contents.reset();

  task_environment()->FastForwardBy(kDeadline);
  EXPECT_TRUE(prerenderer().is_refill_pending_for_testing());
  EXPECT_FALSE(prerenderer().has_spare());

  task_environment()->FastForwardBy(kDelay);
  EXPECT_TRUE(prerenderer().has_spare());
  EXPECT_FALSE(prerenderer().is_refill_pending_for_testing());
}

TEST_F(WebUIContentsPrerendererTest, AdoptsSpare) {
  Refill();
  content::WebContents* spare = prerenderer().spare_for_testing();

  WebUIContentsPrerenderer::TakeOutcome outcome = prerenderer().Take();
  EXPECT_EQ(spare, outcome.contents.get());
  EXPECT_EQ(TakeResult::kAdopted, outcome.result);
  EXPECT_FALSE(prerenderer().has_spare());
}

TEST_F(WebUIContentsPrerendererTest, DestroysRejectedSpareSynchronously) {
  Refill();
  content::WebContentsDestroyedWatcher watcher(
      prerenderer().spare_for_testing());
  delegate().spare_is_current = false;

  WebUIContentsPrerenderer::TakeOutcome outcome = prerenderer().Take();
  EXPECT_FALSE(outcome.contents);
  EXPECT_EQ(TakeResult::kDroppedStale, outcome.result);
  EXPECT_FALSE(prerenderer().has_spare());
  EXPECT_TRUE(watcher.IsDestroyed());
}

TEST_F(WebUIContentsPrerendererTest, DropsPaintedSpare) {
  Refill();
  content::WebContentsTester::For(prerenderer().spare_for_testing())
      ->TestDidFirstVisuallyNonEmptyPaint();

  WebUIContentsPrerenderer::TakeOutcome outcome = prerenderer().Take();
  EXPECT_FALSE(outcome.contents);
  EXPECT_EQ(TakeResult::kDroppedAlreadyPainted, outcome.result);
  EXPECT_FALSE(prerenderer().has_spare());
  EXPECT_EQ(0, delegate().check_count);
}

TEST_F(WebUIContentsPrerendererTest, ResetClearsSpareAndCancelsRefill) {
  Refill();
  content::WebContentsDestroyedWatcher watcher(
      prerenderer().spare_for_testing());
  prerenderer().ScheduleRefill(nullptr);

  prerenderer().Reset();
  EXPECT_TRUE(watcher.IsDestroyed());
  EXPECT_FALSE(prerenderer().has_spare());
  EXPECT_FALSE(prerenderer().is_refill_pending_for_testing());
  task_environment()->FastForwardBy(kDeadline + kDelay);
  EXPECT_FALSE(prerenderer().has_spare());
  EXPECT_EQ(1, delegate().create_count);
}

TEST_F(WebUIContentsPrerendererTest, NewerRefillReplacesPendingOne) {
  prerenderer().ScheduleRefill(nullptr);
  task_environment()->FastForwardBy(kDeadline);
  // The first refill now waits for its delay. A newer one restarts the wait.
  prerenderer().ScheduleRefill(nullptr);
  task_environment()->FastForwardBy(kDelay);
  EXPECT_FALSE(prerenderer().has_spare());

  task_environment()->FastForwardBy(kDeadline);
  EXPECT_TRUE(prerenderer().has_spare());
  EXPECT_EQ(1, delegate().create_count);
}

TEST_F(WebUIContentsPrerendererTest, SkipsRefillWhenDelegateCreatesNothing) {
  delegate().can_create_spare = false;
  prerenderer().ScheduleRefill(nullptr);
  task_environment()->FastForwardBy(kDeadline + kDelay);
  EXPECT_EQ(1, delegate().create_count);
  EXPECT_FALSE(prerenderer().has_spare());
  EXPECT_FALSE(prerenderer().is_refill_pending_for_testing());
}

TEST_F(WebUIContentsPrerendererTest, SkipsRefillUnderMemoryPressure) {
  prerenderer().ScheduleRefill(nullptr);
  test_memory_consumer_registry_.NotifyUpdateMemoryLimit(
      base::MemoryLimit::CriticalPressureThreshold());
  task_environment()->FastForwardBy(kDeadline + kDelay);
  EXPECT_EQ(0, delegate().create_count);
  EXPECT_FALSE(prerenderer().has_spare());
  EXPECT_FALSE(prerenderer().is_refill_pending_for_testing());
}

TEST_F(WebUIContentsPrerendererTest, RunsAboveSkipMemoryLimit) {
  prerenderer().ScheduleRefill(nullptr);
  // Moderate pressure is above the critical skip limit, so the refill runs.
  test_memory_consumer_registry_.NotifyUpdateMemoryLimit(
      base::MemoryLimit::ModeratePressureThreshold());
  task_environment()->FastForwardBy(kDeadline + kDelay);
  EXPECT_EQ(1, delegate().create_count);
  EXPECT_TRUE(prerenderer().has_spare());
  EXPECT_FALSE(prerenderer().is_refill_pending_for_testing());
}

}  // namespace
