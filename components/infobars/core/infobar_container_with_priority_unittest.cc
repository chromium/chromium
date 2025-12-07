// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/infobar_container_with_priority.h"

#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/infobars/core/features.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace infobars {
namespace {

class PriorityDelegate : public InfoBarDelegate {
 public:
  explicit PriorityDelegate(
      InfobarPriority priority,
      InfoBarIdentifier id = ALTERNATE_NAV_INFOBAR_DELEGATE)
      : priority_(priority), id_(id) {}
  ~PriorityDelegate() override = default;

  bool EqualsDelegate(InfoBarDelegate* /*delegate*/) const override {
    return false;
  }

  InfoBarIdentifier GetIdentifier() const override { return id_; }
  bool ShouldExpire(const NavigationDetails&) const override { return false; }
  InfobarPriority GetPriority() const override { return priority_; }

 private:
  InfobarPriority priority_;
  InfoBarIdentifier id_;
};

class TestInfoBar : public InfoBar {
 public:
  explicit TestInfoBar(std::unique_ptr<InfoBarDelegate> delegate)
      : InfoBar(std::move(delegate)) {}
  ~TestInfoBar() override = default;
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

  size_t add_calls() const { return add_calls_; }
  size_t remove_calls() const { return remove_calls_; }
  size_t visible_count() const { return visible_infobars_.size(); }

  bool IsCurrentlyVisible(const InfoBar* infobar) const {
    return base::Contains(visible_infobars_, infobar);
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

 private:
  size_t add_calls_ = 0;
  size_t remove_calls_ = 0;
  std::vector<raw_ptr<InfoBar>> visible_infobars_;
};

class TestManager : public InfoBarManager {
 public:
  TestManager() { set_animations_enabled(false); }
  ~TestManager() override = default;

  int GetActiveEntryID() override { return 0; }
  void OpenURL(const GURL&, WindowOpenDisposition) override {}

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

static void EnableWithCaps(base::test::ScopedFeatureList& feature_list,
                           int critical_cap,
                           int default_cap,
                           int low_cap) {
  feature_list.InitAndEnableFeatureWithParameters(
      kInfobarPrioritization,
      {{kMaxVisibleCritical.name, base::NumberToString(critical_cap)},
       {kMaxVisibleDefault.name, base::NumberToString(default_cap)},
       {kMaxVisibleLow.name, base::NumberToString(low_cap)},
       {kMaxLowQueued.name, "3"}});
}

class InfoBarContainerWithPriorityTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestPriorityContainer::MockDelegate delegate_;
  base::HistogramTester histogram_tester_;
};

TEST_F(InfoBarContainerWithPriorityTest, DefaultIsSingleVisibleThenFIFO) {
  base::test::ScopedFeatureList feature_list;
  EnableWithCaps(feature_list, /*critical_cap=*/2, /*default_cap=*/1,
                 /*low_cap=*/1);

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
  base::test::ScopedFeatureList feature_list;
  EnableWithCaps(feature_list, /*critical_cap=*/2, /*default_cap=*/1,
                 /*low_cap=*/1);

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
  base::test::ScopedFeatureList feature_list;
  EnableWithCaps(feature_list, /*critical_cap=*/2, /*default_cap=*/1,
                 /*low_cap=*/1);

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
  base::test::ScopedFeatureList feature_list;
  EnableWithCaps(feature_list, /*critical_cap=*/2, /*default_cap=*/1,
                 /*low_cap=*/1);

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
  base::test::ScopedFeatureList feature_list;
  EnableWithCaps(feature_list, /*critical_cap=*/2, /*default_cap=*/1,
                 /*low_cap=*/1);

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
  base::test::ScopedFeatureList feature_list;
  EnableWithCaps(feature_list, /*critical_cap=*/2, /*default_cap=*/1,
                 /*low_cap=*/1);

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
  base::test::ScopedFeatureList feature_list;
  EnableWithCaps(feature_list, /*critical_cap=*/1, /*default_cap=*/1,
                 /*low_cap=*/1);

  TestPriorityContainer container(&delegate_);
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
  base::test::ScopedFeatureList feature_list;
  EnableWithCaps(feature_list, /*critical_cap=*/2, /*default_cap=*/1,
                 /*low_cap=*/1);

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
  base::test::ScopedFeatureList feature_list;
  EnableWithCaps(feature_list, /*critical_cap=*/2, /*default_cap=*/1,
                 /*low_cap=*/1);

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
  base::test::ScopedFeatureList feature_list;
  EnableWithCaps(feature_list, /*critical_cap=*/1, /*default_cap=*/1,
                 /*low_cap=*/1);

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
  base::test::ScopedFeatureList feature_list;
  EnableWithCaps(feature_list, /*critical_cap=*/1, /*default_cap=*/1,
                 /*low_cap=*/1);

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
  base::test::ScopedFeatureList feature_list;
  EnableWithCaps(feature_list, /*critical_cap=*/1, /*default_cap=*/1,
                 /*low_cap=*/1);

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
}

}  // namespace
}  // namespace infobars
