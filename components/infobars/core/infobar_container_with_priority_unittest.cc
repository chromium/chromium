// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/infobar_container_with_priority.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace infobars {
namespace {

class PriorityDelegate : public InfoBarDelegate {
 public:
  explicit PriorityDelegate(
      InfobarPriority priority,
      InfoBarIdentifier id = ALTERNATE_NAV_INFOBAR_DELEGATE,
      bool closeable = true)
      : priority_(priority), id_(id), closeable_(closeable) {}
  ~PriorityDelegate() override = default;

  // When true, this delegate reports every other delegate as its equal, to
  // exercise the duplicate-detection paths.
  void set_equals_everything(bool value) { equals_everything_ = value; }

  bool EqualsDelegate(InfoBarDelegate* /*delegate*/) const override {
    return equals_everything_;
  }

  InfoBarIdentifier GetIdentifier() const override { return id_; }
  bool ShouldExpire(const NavigationDetails&) const override { return false; }
  InfobarPriority GetPriority() const override { return priority_; }
  bool IsCloseable() const override { return closeable_; }

 private:
  InfobarPriority priority_;
  InfoBarIdentifier id_;
  bool closeable_;
  bool equals_everything_ = false;
};

class TestInfoBar : public InfoBar {
 public:
  explicit TestInfoBar(std::unique_ptr<InfoBarDelegate> delegate)
      : InfoBar(std::move(delegate)) {}
  ~TestInfoBar() override = default;
};

class MockInfoBar : public InfoBar {
 public:
  explicit MockInfoBar(std::unique_ptr<InfoBarDelegate> delegate)
      : InfoBar(std::move(delegate)) {}
  ~MockInfoBar() override = default;

  MOCK_METHOD1(PlatformSpecificShow, void(bool animate));
};

class TestPriorityContainer : public InfoBarContainerWithPriority {
 public:
  class MockDelegate : public InfoBarContainer::Delegate {
   public:
    void InfoBarContainerStateChanged(bool) override {}
  };

  explicit TestPriorityContainer(Delegate* delegate)
      : InfoBarContainerWithPriority(delegate) {}
  ~TestPriorityContainer() override = default;

  void SetTestingCaps(size_t critical_cap, size_t default_cap, size_t low_cap) {
    use_testing_caps_ = true;
    critical_cap_ = critical_cap;
    default_cap_ = default_cap;
    low_cap_ = low_cap;
  }

  size_t add_calls() const { return add_calls_; }
  size_t remove_calls() const { return remove_calls_; }
  size_t visible_count() const { return visible_infobars_.size(); }

  // Every pending-count value reported through
  // PlatformSpecificPendingInfoBarCountChanged(), in order.
  const std::vector<size_t>& pending_count_notifications() const {
    return pending_count_notifications_;
  }

  bool IsCurrentlyVisible(const InfoBar* infobar) const {
    return std::ranges::contains(visible_infobars_, infobar);
  }

 protected:
  void PlatformSpecificAddInfoBar(InfoBar* infobar, size_t) override {
    ++add_calls_;
    visible_infobars_.push_back(infobar);
  }

  void PlatformSpecificRemoveInfoBar(InfoBar* infobar) override {
    ++remove_calls_;
    auto it =
        std::find(visible_infobars_.begin(), visible_infobars_.end(), infobar);
    if (it != visible_infobars_.end()) {
      visible_infobars_.erase(it);
    }
  }

  void PlatformSpecificPendingInfoBarCountChanged(size_t count) override {
    pending_count_notifications_.push_back(count);
  }

  size_t GetInfoBarPriorityCapFor(
      InfoBarDelegate::InfobarPriority priority) const override {
    if (use_testing_caps_) {
      switch (priority) {
        case InfoBarDelegate::InfobarPriority::kCriticalSecurity:
          return critical_cap_;
        case InfoBarDelegate::InfobarPriority::kDefault:
          return default_cap_;
        case InfoBarDelegate::InfobarPriority::kLow:
          return low_cap_;
      }
    }
    return InfoBarContainerWithPriority::GetInfoBarPriorityCapFor(priority);
  }

 private:
  size_t add_calls_ = 0;
  size_t remove_calls_ = 0;
  std::vector<raw_ptr<InfoBar>> visible_infobars_;
  std::vector<size_t> pending_count_notifications_;
  bool use_testing_caps_ = false;
  size_t critical_cap_ = 0;
  size_t default_cap_ = 0;
  size_t low_cap_ = 0;
};

class TestManager : public InfoBarManager {
 public:
  TestManager() { set_animations_enabled(false); }
  ~TestManager() override = default;

  int GetActiveEntryID() override { return 0; }
  void OpenURL(const GURL&,
               WindowOpenDisposition,
               const std::string&) override {}

  using InfoBarManager::AddInfoBar;
  using InfoBarManager::RemoveInfoBar;
  using InfoBarManager::ReplaceInfoBar;
};

static TestInfoBar* AddInfoBar(TestManager* test_manager,
                               InfoBarDelegate::InfobarPriority priority) {
  return static_cast<TestInfoBar*>(
      test_manager->AddInfoBar(std::make_unique<TestInfoBar>(
          std::make_unique<PriorityDelegate>(priority))));
}

static TestInfoBar* AddNonCloseableInfoBar(
    TestManager* test_manager,
    InfoBarDelegate::InfobarPriority priority) {
  return static_cast<TestInfoBar*>(test_manager->AddInfoBar(
      std::make_unique<TestInfoBar>(std::make_unique<PriorityDelegate>(
          priority, InfoBarDelegate::LOCAL_TEST_POLICIES_APPLIED_INFOBAR,
          /*closeable=*/false))));
}

class InfoBarContainerWithPriorityTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestPriorityContainer::MockDelegate delegate_;
  base::HistogramTester histogram_tester_;
};

TEST_F(InfoBarContainerWithPriorityTest, DefaultIsSingleVisibleThenFIFO) {

  TestPriorityContainer container(&delegate_);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);

  auto* first_default =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  auto* second_default =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  auto* third_default =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);

  EXPECT_EQ(1u, container.visible_count());
  EXPECT_TRUE(container.IsCurrentlyVisible(first_default));
  EXPECT_FALSE(container.IsCurrentlyVisible(second_default));
  EXPECT_FALSE(container.IsCurrentlyVisible(third_default));

  manager.RemoveInfoBar(first_default);
  EXPECT_TRUE(container.IsCurrentlyVisible(second_default));
  EXPECT_FALSE(container.IsCurrentlyVisible(third_default));

  manager.RemoveInfoBar(second_default);
  EXPECT_TRUE(container.IsCurrentlyVisible(third_default));

  manager.RemoveInfoBar(third_default);
  EXPECT_EQ(0u, container.visible_count());

  histogram_tester_.ExpectTotalCount("InfoBar.Prioritization.QueueSize", 3);
}

TEST_F(InfoBarContainerWithPriorityTest,
       LowQueuedWhileDefaultVisibleOrPending) {

  TestPriorityContainer container(&delegate_);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);

  auto* first_default_infobar =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  auto* low_priority_infobar =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kLow);
  auto* second_default_infobar =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);

  EXPECT_TRUE(container.IsCurrentlyVisible(first_default_infobar));
  EXPECT_FALSE(container.IsCurrentlyVisible(low_priority_infobar));
  EXPECT_FALSE(container.IsCurrentlyVisible(second_default_infobar));

  manager.RemoveInfoBar(first_default_infobar);
  EXPECT_TRUE(container.IsCurrentlyVisible(second_default_infobar));
  EXPECT_FALSE(container.IsCurrentlyVisible(low_priority_infobar));

  manager.RemoveInfoBar(second_default_infobar);
  EXPECT_TRUE(container.IsCurrentlyVisible(low_priority_infobar));

  manager.RemoveInfoBar(low_priority_infobar);
}

TEST_F(InfoBarContainerWithPriorityTest, QueueOrderingFIFOWithinPriority) {

  TestPriorityContainer container(&delegate_);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);

  auto* first_low_infobar =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kLow);
  EXPECT_TRUE(container.IsCurrentlyVisible(first_low_infobar));

  auto* default_infobar =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  EXPECT_TRUE(container.IsCurrentlyVisible(first_low_infobar));
  EXPECT_FALSE(container.IsCurrentlyVisible(default_infobar));

  auto* second_low_infobar =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kLow);

  manager.RemoveInfoBar(first_low_infobar);
  EXPECT_TRUE(container.IsCurrentlyVisible(default_infobar));
  EXPECT_FALSE(container.IsCurrentlyVisible(second_low_infobar));

  manager.RemoveInfoBar(default_infobar);
  EXPECT_TRUE(container.IsCurrentlyVisible(second_low_infobar));

  manager.RemoveInfoBar(second_low_infobar);
}

TEST_F(InfoBarContainerWithPriorityTest, CriticalStacksUpToCap) {

  TestPriorityContainer container(&delegate_);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);

  auto* critical_infobar_1 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kCriticalSecurity);
  auto* critical_infobar_2 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kCriticalSecurity);
  auto* critical_infobar_3 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kCriticalSecurity);

  EXPECT_EQ(2u, container.visible_count());
  EXPECT_TRUE(container.IsCurrentlyVisible(critical_infobar_1));
  EXPECT_TRUE(container.IsCurrentlyVisible(critical_infobar_2));
  EXPECT_FALSE(container.IsCurrentlyVisible(critical_infobar_3));

  manager.RemoveInfoBar(critical_infobar_1);
  EXPECT_EQ(2u, container.visible_count());
  EXPECT_TRUE(container.IsCurrentlyVisible(critical_infobar_3));
  EXPECT_TRUE(container.IsCurrentlyVisible(critical_infobar_2));

  manager.RemoveInfoBar(critical_infobar_2);
  manager.RemoveInfoBar(critical_infobar_3);
}

TEST_F(InfoBarContainerWithPriorityTest,
       DefaultDoesNotSurfaceWhileCriticalVisibleOrQueued) {

  TestPriorityContainer container(&delegate_);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);

  auto* critical_infobar_1 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kCriticalSecurity);
  EXPECT_TRUE(container.IsCurrentlyVisible(critical_infobar_1));

  auto* default_infobar_1 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  EXPECT_FALSE(container.IsCurrentlyVisible(default_infobar_1));

  auto* critical_infobar_2 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kCriticalSecurity);
  EXPECT_TRUE(container.IsCurrentlyVisible(critical_infobar_2));

  auto* default_infobar_2 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  EXPECT_FALSE(container.IsCurrentlyVisible(default_infobar_1));
  EXPECT_FALSE(container.IsCurrentlyVisible(default_infobar_2));

  manager.RemoveInfoBar(critical_infobar_1);
  manager.RemoveInfoBar(critical_infobar_2);

  EXPECT_TRUE(container.IsCurrentlyVisible(default_infobar_1));
  EXPECT_FALSE(container.IsCurrentlyVisible(default_infobar_2));

  manager.RemoveInfoBar(default_infobar_1);
  EXPECT_TRUE(container.IsCurrentlyVisible(default_infobar_2));

  manager.RemoveInfoBar(default_infobar_2);
}

TEST_F(InfoBarContainerWithPriorityTest, QueuedInfoBarsArePromotedInFIFOOrder) {

  TestPriorityContainer container(&delegate_);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);

  auto* default_infobar_1 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  auto* default_infobar_2 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  auto* default_infobar_3 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);

  EXPECT_EQ(1u, container.visible_count());
  EXPECT_TRUE(container.IsCurrentlyVisible(default_infobar_1));
  EXPECT_FALSE(container.IsCurrentlyVisible(default_infobar_2));
  EXPECT_FALSE(container.IsCurrentlyVisible(default_infobar_3));

  manager.RemoveInfoBar(default_infobar_1);
  EXPECT_EQ(1u, container.visible_count());
  EXPECT_TRUE(container.IsCurrentlyVisible(default_infobar_2));
  EXPECT_FALSE(container.IsCurrentlyVisible(default_infobar_3));

  manager.RemoveInfoBar(default_infobar_2);
  EXPECT_EQ(1u, container.visible_count());
  EXPECT_TRUE(container.IsCurrentlyVisible(default_infobar_3));

  manager.RemoveInfoBar(default_infobar_3);
}

TEST_F(InfoBarContainerWithPriorityTest, CriticalCapOne) {
  TestPriorityContainer container(&delegate_);
  container.SetTestingCaps(/*critical_cap=*/1, /*default_cap=*/1,
                           /*low_cap=*/1);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);

  auto* critical_infobar_1 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kCriticalSecurity);
  auto* critical_infobar_2 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kCriticalSecurity);

  EXPECT_EQ(1u, container.visible_count());
  EXPECT_TRUE(container.IsCurrentlyVisible(critical_infobar_1));
  EXPECT_FALSE(container.IsCurrentlyVisible(critical_infobar_2));

  manager.RemoveInfoBar(critical_infobar_1);
  EXPECT_TRUE(container.IsCurrentlyVisible(critical_infobar_2));

  manager.RemoveInfoBar(critical_infobar_2);
}

TEST_F(InfoBarContainerWithPriorityTest, LowNeverSurfacesAheadOfQueuedDefault) {

  TestPriorityContainer container(&delegate_);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);

  auto* low_infobar_1 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kLow);
  EXPECT_TRUE(container.IsCurrentlyVisible(low_infobar_1));

  auto* default_infobar =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  auto* low_infobar_2 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kLow);

  EXPECT_TRUE(container.IsCurrentlyVisible(low_infobar_1));
  EXPECT_FALSE(container.IsCurrentlyVisible(default_infobar));
  EXPECT_FALSE(container.IsCurrentlyVisible(low_infobar_2));

  manager.RemoveInfoBar(low_infobar_1);
  EXPECT_TRUE(container.IsCurrentlyVisible(default_infobar));
  EXPECT_FALSE(container.IsCurrentlyVisible(low_infobar_2));

  manager.RemoveInfoBar(default_infobar);
  EXPECT_TRUE(container.IsCurrentlyVisible(low_infobar_2));

  manager.RemoveInfoBar(low_infobar_2);
}

TEST_F(InfoBarContainerWithPriorityTest, UmaQueueSizeRecorded) {

  TestPriorityContainer container(&delegate_);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);

  auto* default_infobar_1 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  auto* default_infobar_2 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  auto* low_priority_infobar =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kLow);

  // At this point, only the first infobar's add triggered a sample.
  // The queue has [default_infobar_2, low_priority_infobar].
  histogram_tester_.ExpectTotalCount("InfoBar.Prioritization.QueueSize", 1);
  histogram_tester_.ExpectBucketCount("InfoBar.Prioritization.QueueSize", 0,
                                      1);  // Queue was empty.

  // Removing default_infobar_1 promotes default_infobar_2. The queue now has
  // [low_priority_infobar]. This promotion triggers a second sample.
  manager.RemoveInfoBar(default_infobar_1);
  histogram_tester_.ExpectTotalCount("InfoBar.Prioritization.QueueSize", 2);
  // Queue had 1 item when d2 was promoted.
  histogram_tester_.ExpectBucketCount("InfoBar.Prioritization.QueueSize", 1, 1);
  // Removing default_infobar_2 promotes low_priority_infobar. The queue is now
  // empty. This promotion triggers a third sample.
  manager.RemoveInfoBar(default_infobar_2);
  histogram_tester_.ExpectTotalCount("InfoBar.Prioritization.QueueSize", 3);
  // Queue was empty when l1 was promoted.
  histogram_tester_.ExpectBucketCount("InfoBar.Prioritization.QueueSize", 0, 2);

  manager.RemoveInfoBar(low_priority_infobar);
}

TEST_F(InfoBarContainerWithPriorityTest, ReplaceDoesNotPromoteFromQueue) {

  TestPriorityContainer container(&delegate_);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);

  auto* default_infobar =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  EXPECT_TRUE(container.IsCurrentlyVisible(default_infobar));
  EXPECT_EQ(1u, container.visible_count());

  auto* low_priority_infobar =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kLow);
  EXPECT_FALSE(container.IsCurrentlyVisible(low_priority_infobar));
  EXPECT_EQ(1u, container.visible_count());

  auto critical_infobar_unique =
      std::make_unique<TestInfoBar>(std::make_unique<PriorityDelegate>(
          InfoBarDelegate::InfobarPriority::kCriticalSecurity));
  auto* critical_infobar = critical_infobar_unique.get();
  manager.ReplaceInfoBar(default_infobar, std::move(critical_infobar_unique));

  EXPECT_TRUE(container.IsCurrentlyVisible(critical_infobar));
  EXPECT_FALSE(container.IsCurrentlyVisible(default_infobar));
  EXPECT_FALSE(container.IsCurrentlyVisible(low_priority_infobar));
  EXPECT_EQ(1u, container.visible_count());

  manager.RemoveInfoBar(critical_infobar);
  critical_infobar = nullptr;

  EXPECT_TRUE(container.IsCurrentlyVisible(low_priority_infobar));
  EXPECT_EQ(1u, container.visible_count());

  manager.RemoveInfoBar(low_priority_infobar);
}

TEST_F(InfoBarContainerWithPriorityTest, UmaWaitTimeRecorded) {

  TestPriorityContainer container(&delegate_);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);

  auto* blocking_infobar =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  auto* queued_default =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);

  auto* queued_low =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kLow);

  task_environment_.FastForwardBy(base::Seconds(2));

  // Remove blocking infobar. This should promote `queued_default`. Wait time
  // for Default should be approx 2 seconds.
  manager.RemoveInfoBar(blocking_infobar);

  histogram_tester_.ExpectUniqueTimeSample(
      "InfoBar.Prioritization.DefaultWaitTime", base::Seconds(2), 1);

  // `queued_low` is still in the queue.
  // We wait another 3 seconds (Total wait for low = 5 seconds).
  task_environment_.FastForwardBy(base::Seconds(3));

  // 5. Remove the promoted default infobar.
  // This should promote `queued_low`.
  manager.RemoveInfoBar(queued_default);

  histogram_tester_.ExpectUniqueTimeSample("InfoBar.Prioritization.LowWaitTime",
                                           base::Seconds(5), 1);

  manager.RemoveInfoBar(queued_low);
}

TEST_F(InfoBarContainerWithPriorityTest, UmaStarvedCountRecorded) {

  TestPriorityContainer container(&delegate_);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);

  // 1. Fill the slots so subsequent adds are queued.
  AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);

  // 2. Add 3 queued infobars.
  AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kLow);
  AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kLow);

  // 3. Simulate Tab Close / Manager Switch.
  // This triggers ChangeInfoBarManager(nullptr), which clears the queue.
  // The metric should record the number of items dropped (starved).
  container.ChangeInfoBarManager(nullptr);

  histogram_tester_.ExpectUniqueSample("InfoBar.Prioritization.StarvedCount", 3,
                                       1);
  histogram_tester_.ExpectBucketCount(
      "InfoBar.Prioritization.Starved",
      InfoBarDelegate::ALTERNATE_NAV_INFOBAR_DELEGATE, 3);
}

TEST_F(InfoBarContainerWithPriorityTest, PendingCountNotifiedOnQueueChanges) {
  TestPriorityContainer container(&delegate_);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);

  EXPECT_EQ(0u, container.PendingInfoBarCount());
  EXPECT_TRUE(container.pending_count_notifications().empty());

  // The first default infobar is shown directly; the queue stays empty.
  auto* default_infobar_1 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  EXPECT_TRUE(container.pending_count_notifications().empty());

  // The next two are queued, notifying counts 1 and 2.
  auto* default_infobar_2 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  auto* default_infobar_3 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  EXPECT_EQ(2u, container.PendingInfoBarCount());
  EXPECT_EQ((std::vector<size_t>{1u, 2u}),
            container.pending_count_notifications());

  // Removing the visible infobar promotes one from the queue.
  manager.RemoveInfoBar(default_infobar_1);
  EXPECT_EQ((std::vector<size_t>{1u, 2u, 1u}),
            container.pending_count_notifications());

  // Removing an infobar straight out of the queue also notifies.
  manager.RemoveInfoBar(default_infobar_3);
  EXPECT_EQ((std::vector<size_t>{1u, 2u, 1u, 0u}),
            container.pending_count_notifications());

  // Removing the last visible infobar leaves the queue empty; no redundant
  // notification is sent.
  manager.RemoveInfoBar(default_infobar_2);
  EXPECT_EQ(4u, container.pending_count_notifications().size());
}

TEST_F(InfoBarContainerWithPriorityTest, PendingCountNotifiedOnManagerCleared) {
  TestPriorityContainer container(&delegate_);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);

  AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kLow);
  EXPECT_EQ(2u, container.PendingInfoBarCount());

  // Detaching the manager drops (starves) the queue, which must be reported
  // so any visual queue indicator is hidden.
  container.ChangeInfoBarManager(nullptr);
  EXPECT_EQ(0u, container.PendingInfoBarCount());
  ASSERT_FALSE(container.pending_count_notifications().empty());
  EXPECT_EQ(0u, container.pending_count_notifications().back());
}

TEST_F(InfoBarContainerWithPriorityTest,
       ReplaceQueuedInfoBarKeepsQueueConsistent) {
  TestPriorityContainer container(&delegate_);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);

  auto* visible_default =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  auto* queued_default =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  ASSERT_TRUE(container.IsCurrentlyVisible(visible_default));
  ASSERT_FALSE(container.IsCurrentlyVisible(queued_default));
  ASSERT_EQ(1u, container.PendingInfoBarCount());

  // Replace the infobar while it is still queued: the old entry is dropped
  // and the replacement re-queued, without touching the base container.
  auto replacement_unique =
      std::make_unique<TestInfoBar>(std::make_unique<PriorityDelegate>(
          InfoBarDelegate::InfobarPriority::kDefault));
  auto* replacement = replacement_unique.get();
  manager.ReplaceInfoBar(queued_default, std::move(replacement_unique));

  EXPECT_TRUE(container.IsCurrentlyVisible(visible_default));
  EXPECT_FALSE(container.IsCurrentlyVisible(replacement));
  EXPECT_EQ(1u, container.PendingInfoBarCount());

  // The replacement is promoted normally once the visible infobar goes away.
  manager.RemoveInfoBar(visible_default);
  EXPECT_TRUE(container.IsCurrentlyVisible(replacement));
  EXPECT_EQ(0u, container.PendingInfoBarCount());

  manager.RemoveInfoBar(replacement);
}

TEST_F(InfoBarContainerWithPriorityTest,
       PendingCountNotifiedOnManagerSwapWithEqualCounts) {
  TestPriorityContainer container(&delegate_);
  TestManager manager_a;
  container.ChangeInfoBarManager(&manager_a);

  AddInfoBar(&manager_a, InfoBarDelegate::InfobarPriority::kDefault);
  AddInfoBar(&manager_a, InfoBarDelegate::InfobarPriority::kDefault);
  ASSERT_EQ(1u, container.PendingInfoBarCount());
  ASSERT_EQ((std::vector<size_t>{1u}), container.pending_count_notifications());

  // The second manager holds the same shape before the container observes
  // it: one bar to admit, one to queue.
  TestManager manager_b;
  manager_b.AddInfoBar(
      std::make_unique<TestInfoBar>(std::make_unique<PriorityDelegate>(
          InfoBarDelegate::InfobarPriority::kDefault)));
  manager_b.AddInfoBar(
      std::make_unique<TestInfoBar>(std::make_unique<PriorityDelegate>(
          InfoBarDelegate::InfobarPriority::kDefault)));

  // Swapping managers rebuilds platform state, so the equal pending count is
  // still reported: once as the teardown's 0, once for the new queue.
  container.ChangeInfoBarManager(&manager_b);
  EXPECT_EQ(1u, container.PendingInfoBarCount());
  EXPECT_EQ((std::vector<size_t>{1u, 0u, 1u}),
            container.pending_count_notifications());

  container.ChangeInfoBarManager(nullptr);
}

TEST_F(InfoBarContainerWithPriorityTest,
       ReplaceQueuedInfoBarTracksDuplicateReplacement) {
  TestPriorityContainer container(&delegate_);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);

  auto* visible_default =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  auto* queued_1 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  auto* queued_2 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  ASSERT_EQ(2u, container.PendingInfoBarCount());

  // Make the remaining queued bar consider everything its equal.
  // ReplaceInfoBar performs no EqualsDelegate dedup, so the container must
  // keep tracking the replacement rather than dropping it as a duplicate.
  static_cast<PriorityDelegate*>(queued_1->delegate())
      ->set_equals_everything(true);

  auto replacement_unique =
      std::make_unique<TestInfoBar>(std::make_unique<PriorityDelegate>(
          InfoBarDelegate::InfobarPriority::kDefault));
  auto* replacement = replacement_unique.get();
  manager.ReplaceInfoBar(queued_2, std::move(replacement_unique));
  EXPECT_EQ(2u, container.PendingInfoBarCount());

  // The full queue drains in order with no CHECK failures.
  manager.RemoveInfoBar(visible_default);
  EXPECT_TRUE(container.IsCurrentlyVisible(queued_1));
  manager.RemoveInfoBar(queued_1);
  EXPECT_TRUE(container.IsCurrentlyVisible(replacement));
  manager.RemoveInfoBar(replacement);
  EXPECT_EQ(0u, container.PendingInfoBarCount());
}

TEST_F(InfoBarContainerWithPriorityTest, NoAnimationOnManagerChange) {

  TestPriorityContainer container(&delegate_);
  TestManager manager;

  auto delegate = std::make_unique<PriorityDelegate>(
      InfoBarDelegate::InfobarPriority::kDefault);
  auto mock_infobar_ptr = std::make_unique<MockInfoBar>(std::move(delegate));
  MockInfoBar* mock_infobar = mock_infobar_ptr.get();

  manager.AddInfoBar(std::move(mock_infobar_ptr));

  EXPECT_CALL(*mock_infobar, PlatformSpecificShow(false)).Times(1);

  container.ChangeInfoBarManager(&manager);
  container.ChangeInfoBarManager(nullptr);
}

TEST_F(InfoBarContainerWithPriorityTest, NonCloseableLowDoesNotBlockDefault) {
  TestPriorityContainer container(&delegate_);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);
  auto* persistent_infobar =
      AddNonCloseableInfoBar(&manager, InfoBarDelegate::InfobarPriority::kLow);
  auto* default_infobar =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);

  EXPECT_EQ(2u, container.visible_count());
  EXPECT_TRUE(container.IsCurrentlyVisible(persistent_infobar));
  EXPECT_TRUE(container.IsCurrentlyVisible(default_infobar));
  EXPECT_FALSE(container.HasPendingInfoBars());

  manager.RemoveInfoBar(default_infobar);
  EXPECT_EQ(1u, container.visible_count());
  EXPECT_TRUE(container.IsCurrentlyVisible(persistent_infobar));

  manager.RemoveInfoBar(persistent_infobar);
  EXPECT_EQ(0u, container.visible_count());
}
TEST_F(InfoBarContainerWithPriorityTest,
       NonCloseableDoesNotConsumePriorityCap) {
  TestPriorityContainer container(&delegate_);
  TestManager manager;

  container.ChangeInfoBarManager(&manager);
  auto* persistent_infobar = AddNonCloseableInfoBar(
      &manager, InfoBarDelegate::InfobarPriority::kDefault);
  auto* default_infobar_1 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  auto* default_infobar_2 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);

  EXPECT_TRUE(container.IsCurrentlyVisible(persistent_infobar));
  EXPECT_TRUE(container.IsCurrentlyVisible(default_infobar_1));
  EXPECT_FALSE(container.IsCurrentlyVisible(default_infobar_2));

  manager.RemoveInfoBar(default_infobar_1);
  EXPECT_TRUE(container.IsCurrentlyVisible(persistent_infobar));
  EXPECT_TRUE(container.IsCurrentlyVisible(default_infobar_2));

  manager.RemoveInfoBar(default_infobar_2);
  manager.RemoveInfoBar(persistent_infobar);
}
TEST_F(InfoBarContainerWithPriorityTest, NonCloseableIsNeverQueued) {
  TestPriorityContainer container(&delegate_);
  TestManager manager;

  container.ChangeInfoBarManager(&manager);
  auto* critical_infobar =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kCriticalSecurity);
  auto* default_infobar =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  auto* persistent_infobar =
      AddNonCloseableInfoBar(&manager, InfoBarDelegate::InfobarPriority::kLow);

  EXPECT_TRUE(container.IsCurrentlyVisible(critical_infobar));
  EXPECT_FALSE(container.IsCurrentlyVisible(default_infobar));
  EXPECT_TRUE(container.IsCurrentlyVisible(persistent_infobar));

  manager.RemoveInfoBar(critical_infobar);
  EXPECT_TRUE(container.IsCurrentlyVisible(default_infobar));
  EXPECT_TRUE(container.IsCurrentlyVisible(persistent_infobar));

  manager.RemoveInfoBar(default_infobar);
  manager.RemoveInfoBar(persistent_infobar);
}
// Admission must not depend on the order infobars are re-added on tab switch.
TEST_F(InfoBarContainerWithPriorityTest,
       NonCloseableAndDefaultBothVisibleAfterManagerChange) {
  TestPriorityContainer container(&delegate_);
  TestManager persistent_first;

  auto* persistent_infobar_1 = AddNonCloseableInfoBar(
      &persistent_first, InfoBarDelegate::InfobarPriority::kLow);
  auto* default_infobar_1 =
      AddInfoBar(&persistent_first, InfoBarDelegate::InfobarPriority::kDefault);

  TestManager default_first;
  auto* default_infobar_2 =
      AddInfoBar(&default_first, InfoBarDelegate::InfobarPriority::kDefault);
  auto* persistent_infobar_2 = AddNonCloseableInfoBar(
      &default_first, InfoBarDelegate::InfobarPriority::kLow);

  container.ChangeInfoBarManager(&persistent_first);
  EXPECT_EQ(2u, container.visible_count());
  EXPECT_TRUE(container.IsCurrentlyVisible(persistent_infobar_1));
  EXPECT_TRUE(container.IsCurrentlyVisible(default_infobar_1));

  container.ChangeInfoBarManager(&default_first);
  EXPECT_EQ(2u, container.visible_count());
  EXPECT_TRUE(container.IsCurrentlyVisible(default_infobar_2));
  EXPECT_TRUE(container.IsCurrentlyVisible(persistent_infobar_2));

  default_first.RemoveInfoBar(default_infobar_2);
  EXPECT_EQ(1u, container.visible_count());
  EXPECT_TRUE(container.IsCurrentlyVisible(persistent_infobar_2));
  container.ChangeInfoBarManager(nullptr);
}

TEST_F(InfoBarContainerWithPriorityTest,
       ReplaceVisibleDefaultWithLowPromotesQueuedDefault) {
  TestPriorityContainer container(&delegate_);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);

  auto* default_infobar_1 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  auto* default_infobar_2 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  ASSERT_TRUE(container.IsCurrentlyVisible(default_infobar_1));
  ASSERT_FALSE(container.IsCurrentlyVisible(default_infobar_2));
  ASSERT_EQ(1u, container.PendingInfoBarCount());

  auto low_replacement_unique =
      std::make_unique<TestInfoBar>(std::make_unique<PriorityDelegate>(
          InfoBarDelegate::InfobarPriority::kLow));
  auto* low_replacement = low_replacement_unique.get();
  manager.ReplaceInfoBar(default_infobar_1, std::move(low_replacement_unique));

  EXPECT_EQ(1u, container.visible_count());
  EXPECT_TRUE(container.IsCurrentlyVisible(default_infobar_2));
  EXPECT_FALSE(container.IsCurrentlyVisible(low_replacement));
  EXPECT_EQ(1u, container.PendingInfoBarCount());

  manager.RemoveInfoBar(default_infobar_2);
  EXPECT_EQ(1u, container.visible_count());
  EXPECT_TRUE(container.IsCurrentlyVisible(low_replacement));
  EXPECT_EQ(0u, container.PendingInfoBarCount());

  manager.RemoveInfoBar(low_replacement);
  EXPECT_EQ(0u, container.visible_count());
}

TEST_F(InfoBarContainerWithPriorityTest,
       ReplaceVisibleDefaultWithDefaultKeepsQueuedDefaultWaiting) {
  TestPriorityContainer container(&delegate_);
  TestManager manager;
  container.ChangeInfoBarManager(&manager);

  auto* default_infobar_1 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  auto* default_infobar_2 =
      AddInfoBar(&manager, InfoBarDelegate::InfobarPriority::kDefault);
  ASSERT_TRUE(container.IsCurrentlyVisible(default_infobar_1));
  ASSERT_FALSE(container.IsCurrentlyVisible(default_infobar_2));
  ASSERT_EQ(1u, container.PendingInfoBarCount());

  auto default_replacement_unique =
      std::make_unique<TestInfoBar>(std::make_unique<PriorityDelegate>(
          InfoBarDelegate::InfobarPriority::kDefault));
  auto* default_replacement = default_replacement_unique.get();
  manager.ReplaceInfoBar(default_infobar_1,
                         std::move(default_replacement_unique));

  EXPECT_EQ(1u, container.visible_count());
  EXPECT_TRUE(container.IsCurrentlyVisible(default_replacement));
  EXPECT_FALSE(container.IsCurrentlyVisible(default_infobar_2));
  EXPECT_EQ(1u, container.PendingInfoBarCount());

  manager.RemoveInfoBar(default_replacement);
  EXPECT_TRUE(container.IsCurrentlyVisible(default_infobar_2));
  EXPECT_EQ(0u, container.PendingInfoBarCount());

  manager.RemoveInfoBar(default_infobar_2);
}

}  // namespace
}  // namespace infobars
