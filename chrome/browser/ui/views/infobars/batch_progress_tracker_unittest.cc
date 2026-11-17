// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/batch_progress_tracker.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "url/gurl.h"

namespace {

class FakeInfoBarManager : public infobars::InfoBarManager {
 public:
  FakeInfoBarManager() { set_animations_enabled(false); }
  ~FakeInfoBarManager() override = default;

  using InfoBarManager::set_animations_enabled;

  int GetActiveEntryID() override { return 0; }
  void OpenURL(const GURL& url,
               ui::mojom::WindowOpenDisposition disposition,
               const std::string& text_fragment) override {}
};

class FakeInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  explicit FakeInfoBarDelegate(
      InfobarPriority priority = InfobarPriority::kDefault,
      bool closeable = true)
      : priority_(priority), closeable_(closeable) {}
  ~FakeInfoBarDelegate() override = default;

  InfoBarIdentifier GetIdentifier() const override { return TEST_INFOBAR; }
  InfobarPriority GetPriority() const override { return priority_; }
  bool IsCloseable() const override { return closeable_; }
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override {
    return false;
  }

 private:
  const InfobarPriority priority_;
  const bool closeable_;
};

class ContainerObserver : public infobars::InfoBarManager::Observer {
 public:
  explicit ContainerObserver(infobars::InfoBarManager& manager)
      : manager_(manager) {
    BatchProgressTracker::GetOrCreate(manager);
    observation_.Observe(&manager);
  }

  void set_next_state(infobars::InfoBar* next_host,
                      bool has_staying_eligible_bar,
                      size_t pending_count) {
    next_host_ = next_host;
    has_staying_eligible_bar_ = has_staying_eligible_bar;
    pending_count_ = pending_count;
  }

  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override {
    last_position_ =
        BatchProgressTracker::Get(&*manager_)
            ->ResolvePosition(next_host_, has_staying_eligible_bar_,
                              pending_count_);
  }

  void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                         infobars::InfoBar* new_infobar) override {
    last_position_ =
        BatchProgressTracker::Get(&*manager_)
            ->ResolvePosition(next_host_, has_staying_eligible_bar_,
                              pending_count_);
  }

  BatchProgressTracker::Position last_position() const {
    return last_position_;
  }

 private:
  const raw_ref<infobars::InfoBarManager> manager_;
  raw_ptr<infobars::InfoBar> next_host_ = nullptr;
  bool has_staying_eligible_bar_ = false;
  size_t pending_count_ = 0;
  BatchProgressTracker::Position last_position_;
  base::ScopedObservation<infobars::InfoBarManager,
                          infobars::InfoBarManager::Observer>
      observation_{this};
};

std::unique_ptr<infobars::InfoBar> CreateBar(
    infobars::InfoBarDelegate::InfobarPriority priority =
        infobars::InfoBarDelegate::InfobarPriority::kDefault,
    bool closeable = true) {
  return std::make_unique<infobars::InfoBar>(
      std::make_unique<FakeInfoBarDelegate>(priority, closeable));
}

class CascadingRemovalObserver : public infobars::InfoBarManager::Observer {
 public:
  CascadingRemovalObserver(infobars::InfoBarManager& manager,
                           infobars::InfoBar* trigger,
                           infobars::InfoBar* cascaded)
      : manager_(&manager), trigger_(trigger), cascaded_(cascaded) {
    observation_.Observe(&manager);
  }

  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override {
    if (infobar == trigger_) {
      trigger_ = nullptr;
      if (infobars::InfoBar* to_remove = std::exchange(cascaded_, nullptr)) {
        manager_->RemoveInfoBar(to_remove);
      }
    }
  }

 private:
  const raw_ptr<infobars::InfoBarManager> manager_;
  raw_ptr<infobars::InfoBar> trigger_;
  raw_ptr<infobars::InfoBar> cascaded_;
  base::ScopedObservation<infobars::InfoBarManager,
                          infobars::InfoBarManager::Observer>
      observation_{this};
};

infobars::InfoBar* AddInfoBar(
    FakeInfoBarManager& manager,
    infobars::InfoBarDelegate::InfobarPriority priority =
        infobars::InfoBarDelegate::InfobarPriority::kDefault,
    bool closeable = true) {
  return manager.AddInfoBar(CreateBar(priority, closeable));
}

}  // namespace

TEST(BatchProgressTrackerTest, ProgressesAndResetsAcrossFullBatch) {
  FakeInfoBarManager manager;
  ASSERT_EQ(nullptr, BatchProgressTracker::Get(&manager));

  infobars::InfoBar* infobar_1 = AddInfoBar(manager);
  infobars::InfoBar* infobar_2 = AddInfoBar(manager);
  infobars::InfoBar* infobar_3 = AddInfoBar(manager);

  BatchProgressTracker& tracker = BatchProgressTracker::GetOrCreate(manager);
  EXPECT_EQ(
      (BatchProgressTracker::Position{1, 3}),
      tracker.ResolvePosition(infobar_1, /*has_staying_eligible_bar=*/true,
                              /*pending_count=*/2));

  manager.RemoveInfoBar(infobar_1);
  EXPECT_EQ(
      (BatchProgressTracker::Position{2, 3}),
      tracker.ResolvePosition(infobar_2, /*has_staying_eligible_bar=*/true,
                              /*pending_count=*/1));

  manager.RemoveInfoBar(infobar_2);
  EXPECT_EQ(
      (BatchProgressTracker::Position{3, 3}),
      tracker.ResolvePosition(infobar_3, /*has_staying_eligible_bar=*/true,
                              /*pending_count=*/0));

  // Removing the final bar resets batch state so a subsequent standalone bar
  // does not show "1 of 1".
  manager.RemoveInfoBar(infobar_3);
  EXPECT_EQ((BatchProgressTracker::Position{0, 0}),
            tracker.ResolvePosition(/*host=*/nullptr,
                                    /*has_staying_eligible_bar=*/false,
                                    /*pending_count=*/0));

  infobars::InfoBar* standalone = AddInfoBar(manager);
  EXPECT_EQ((BatchProgressTracker::Position{0, 0}),
            tracker.ResolvePosition(standalone,
                                    /*has_staying_eligible_bar=*/true,
                                    /*pending_count=*/0));
}

TEST(BatchProgressTrackerTest, BackgroundActiveAndQueuedRemovalsStayAccurate) {
  FakeInfoBarManager manager;
  infobars::InfoBar* infobar_1 = AddInfoBar(manager);
  infobars::InfoBar* infobar_2 = AddInfoBar(manager);
  infobars::InfoBar* infobar_3 = AddInfoBar(manager);

  BatchProgressTracker& tracker = BatchProgressTracker::GetOrCreate(manager);
  ASSERT_EQ(
      (BatchProgressTracker::Position{1, 3}),
      tracker.ResolvePosition(infobar_1, /*has_staying_eligible_bar=*/true,
                              /*pending_count=*/2));

  // Advance to "2 of 3", then detach from container (tab goes to background).
  manager.RemoveInfoBar(infobar_1);
  ASSERT_EQ(
      (BatchProgressTracker::Position{2, 3}),
      tracker.ResolvePosition(infobar_2, /*has_staying_eligible_bar=*/true,
                              /*pending_count=*/1));

  // While in background, `infobar_2` (the active bar the user already saw) is
  // removed without any container attached.
  manager.RemoveInfoBar(infobar_2);

  // When the tab re-attaches (in the same or a different browser window),
  // `infobar_3` surfaces accurately as "3 of 3".
  EXPECT_EQ(
      (BatchProgressTracker::Position{3, 3}),
      tracker.ResolvePosition(infobar_3, /*has_staying_eligible_bar=*/true,
                              /*pending_count=*/0));
}

TEST(BatchProgressTrackerTest, BackgroundQueuedBarExpiryAdjustsTotalOnly) {
  FakeInfoBarManager manager;
  infobars::InfoBar* infobar_1 = AddInfoBar(manager);
  infobars::InfoBar* infobar_2 = AddInfoBar(manager);
  infobars::InfoBar* infobar_3 = AddInfoBar(manager);

  BatchProgressTracker& tracker = BatchProgressTracker::GetOrCreate(manager);
  ASSERT_EQ(
      (BatchProgressTracker::Position{1, 3}),
      tracker.ResolvePosition(infobar_1, /*has_staying_eligible_bar=*/true,
                              /*pending_count=*/2));
  manager.RemoveInfoBar(infobar_1);
  ASSERT_EQ(
      (BatchProgressTracker::Position{2, 3}),
      tracker.ResolvePosition(infobar_2, /*has_staying_eligible_bar=*/true,
                              /*pending_count=*/1));

  // `infobar_3` (unseen queued bar) expires while the tab is in the background.
  manager.RemoveInfoBar(infobar_3);

  // Re-attaching shows `infobar_2` as "2 of 2".
  EXPECT_EQ(
      (BatchProgressTracker::Position{2, 2}),
      tracker.ResolvePosition(infobar_2, /*has_staying_eligible_bar=*/true,
                              /*pending_count=*/0));
}

TEST(BatchProgressTrackerTest, PreservesActiveBarAcrossCriticalSuppression) {
  FakeInfoBarManager manager;
  infobars::InfoBar* infobar_1 = AddInfoBar(manager);
  infobars::InfoBar* infobar_2 = AddInfoBar(manager);

  BatchProgressTracker& tracker = BatchProgressTracker::GetOrCreate(manager);
  ASSERT_EQ(
      (BatchProgressTracker::Position{1, 2}),
      tracker.ResolvePosition(infobar_1, /*has_staying_eligible_bar=*/true,
                              /*pending_count=*/1));

  // A critical bar arrives below `infobar_1`, suppressing `host` (`nullptr`)
  // while `infobar_1` stays in the container.
  infobars::InfoBar* critical = AddInfoBar(
      manager, infobars::InfoBarDelegate::InfobarPriority::kCriticalSecurity);
  EXPECT_EQ((BatchProgressTracker::Position{0, 0}),
            tracker.ResolvePosition(/*host=*/nullptr,
                                    /*has_staying_eligible_bar=*/true,
                                    /*pending_count=*/1));

  // Dismissing `infobar_1` while `critical` is still visible advances progress;
  // once `critical` is also dismissed, `infobar_2` surfaces as "2 of 2".
  manager.RemoveInfoBar(infobar_1);
  manager.RemoveInfoBar(critical);
  EXPECT_EQ(
      (BatchProgressTracker::Position{2, 2}),
      tracker.ResolvePosition(infobar_2, /*has_staying_eligible_bar=*/true,
                              /*pending_count=*/0));
}

TEST(BatchProgressTrackerTest,
     ManagerDestructionWithActiveBatchCleansUpSafely) {
  auto manager = std::make_unique<FakeInfoBarManager>();
  infobars::InfoBar* infobar_1 = AddInfoBar(*manager);
  AddInfoBar(*manager);
  AddInfoBar(*manager);

  BatchProgressTracker& tracker = BatchProgressTracker::GetOrCreate(*manager);
  ASSERT_EQ(
      (BatchProgressTracker::Position{1, 3}),
      tracker.ResolvePosition(infobar_1, /*has_staying_eligible_bar=*/true,
                              /*pending_count=*/2));

  // Destroying `manager` with an active batch and cached `active_infobar_` must
  // cleanly clear `active_infobar_`, unregister the observer before
  // `~ObserverList` checks emptiness, and destroy UserData without touching the
  // dying host.
  manager.reset();
}

TEST(BatchProgressTrackerTest,
     ContainerObserverSeesReconciledPositionOnRemovalAndReplacement) {
  FakeInfoBarManager manager;
  ContainerObserver container(manager);

  infobars::InfoBar* infobar_1 = AddInfoBar(manager);
  infobars::InfoBar* infobar_2 = AddInfoBar(manager);

  BatchProgressTracker& tracker = BatchProgressTracker::GetOrCreate(manager);
  ASSERT_EQ(
      (BatchProgressTracker::Position{1, 2}),
      tracker.ResolvePosition(infobar_1, /*has_staying_eligible_bar=*/true,
                              /*pending_count=*/1));

  // Replacing `infobar_1` in place while suppressed by a critical infobar
  // (`host == nullptr`) must transfer `active_infobar_` to
  // `infobar_1_replacement` before `container.OnInfoBarReplaced()` queries
  // `ResolvePosition()`.
  infobars::InfoBar* critical = AddInfoBar(
      manager, infobars::InfoBarDelegate::InfobarPriority::kCriticalSecurity);
  ASSERT_EQ((BatchProgressTracker::Position{0, 0}),
            tracker.ResolvePosition(/*host=*/nullptr,
                                    /*has_staying_eligible_bar=*/true,
                                    /*pending_count=*/1));

  container.set_next_state(/*next_host=*/nullptr,
                           /*has_staying_eligible_bar=*/true,
                           /*pending_count=*/1);
  infobars::InfoBar* infobar_1_replacement =
      manager.ReplaceInfoBar(infobar_1, CreateBar());
  EXPECT_EQ((BatchProgressTracker::Position{0, 0}), container.last_position());

  // Dismissing `critical` reveals `infobar_1_replacement` at "1 of 2";
  // dismissing `infobar_1_replacement` advances `container.OnInfoBarRemoved()`
  // directly to "2 of 2".
  container.set_next_state(infobar_1_replacement,
                           /*has_staying_eligible_bar=*/true,
                           /*pending_count=*/1);
  manager.RemoveInfoBar(critical);
  EXPECT_EQ((BatchProgressTracker::Position{1, 2}), container.last_position());

  container.set_next_state(infobar_2, /*has_staying_eligible_bar=*/true,
                           /*pending_count=*/0);
  manager.RemoveInfoBar(infobar_1_replacement);
  EXPECT_EQ((BatchProgressTracker::Position{2, 2}), container.last_position());
}

TEST(BatchProgressTrackerTest, ReentrantObserverRemovalDuringOnInfoBarRemoved) {
  FakeInfoBarManager manager;
  infobars::InfoBar* infobar_1 = AddInfoBar(manager);
  infobars::InfoBar* infobar_2 = AddInfoBar(manager);
  infobars::InfoBar* infobar_3 = AddInfoBar(manager);

  BatchProgressTracker& tracker = BatchProgressTracker::GetOrCreate(manager);
  ASSERT_EQ(
      (BatchProgressTracker::Position{1, 3}),
      tracker.ResolvePosition(infobar_1, /*has_staying_eligible_bar=*/true,
                              /*pending_count=*/2));

  // Removing `infobar_1` synchronously triggers removal of queued `infobar_2`
  // from inside `OnInfoBarRemoved`; `infobar_3` must settle cleanly as "2 of
  // 2".
  {
    CascadingRemovalObserver cascading(manager, infobar_1, infobar_2);
    manager.RemoveInfoBar(infobar_1);
  }
  EXPECT_EQ(
      (BatchProgressTracker::Position{2, 2}),
      tracker.ResolvePosition(infobar_3, /*has_staying_eligible_bar=*/true,
                              /*pending_count=*/0));
}
