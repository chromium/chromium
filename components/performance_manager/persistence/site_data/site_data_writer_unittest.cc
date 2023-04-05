// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/site_data_writer.h"

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "components/performance_manager/persistence/site_data/site_data_impl.h"
#include "components/performance_manager/public/persistence/site_data/feature_usage.h"
#include "components/performance_manager/test_support/persistence/unittest_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager {

class SiteDataWriterTest : public ::testing::Test {
 protected:
  // The constructors needs to call 'new' directly rather than using the
  // base::MakeRefCounted helper function because the constructor of
  // SiteDataImpl is protected and not visible to
  // base::MakeRefCounted.
  SiteDataWriterTest()
      : test_impl_(base::WrapRefCounted(
            new internal::SiteDataImpl(url::Origin::Create(GURL("foo.com")),
                                       delegate_.GetWeakPtr(),
                                       &data_store_))) {
    SiteDataWriter* writer = new SiteDataWriter(test_impl_.get());
    writer_ = base::WrapUnique(writer);
  }

  SiteDataWriterTest(const SiteDataWriterTest&) = delete;
  SiteDataWriterTest& operator=(const SiteDataWriterTest&) = delete;

  ~SiteDataWriterTest() override = default;

  bool TabIsLoaded() { return test_impl_->IsLoaded(); }

  bool TabIsLoadedAndInBackground() {
    return test_impl_->loaded_tabs_in_background_count_for_testing() != 0U;
  }

  // The mock delegate used by the SiteDataImpl objects
  // created by this class, NiceMock is used to avoid having to set
  // expectations in test cases that don't care about this.
  ::testing::NiceMock<testing::MockSiteDataImplOnDestroyDelegate> delegate_;

  testing::NoopSiteDataStore data_store_;

  // The SiteDataImpl object used in these tests.
  scoped_refptr<internal::SiteDataImpl> test_impl_;

  // A SiteDataWriter object associated with the origin used
  // to create this object.
  std::unique_ptr<SiteDataWriter> writer_;
};

TEST_F(SiteDataWriterTest, TestModifiers) {
  // Make sure that we initially have no information about any of the features
  // and that the site is in an unloaded state.
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            test_impl_->UpdatesFaviconInBackground());
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            test_impl_->UpdatesTitleInBackground());
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            test_impl_->UsesAudioInBackground());

  // Test the OnTabLoaded function.
  EXPECT_FALSE(TabIsLoaded());
  writer_->NotifySiteLoaded(TabVisibility::kBackground);
  EXPECT_TRUE(TabIsLoaded());

  // Test all the modifiers.

  writer_->NotifyUpdatesFaviconInBackground();
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            test_impl_->UpdatesFaviconInBackground());
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            test_impl_->UpdatesTitleInBackground());
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            test_impl_->UsesAudioInBackground());

  writer_->NotifyUpdatesTitleInBackground();
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            test_impl_->UpdatesFaviconInBackground());
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            test_impl_->UpdatesTitleInBackground());
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            test_impl_->UsesAudioInBackground());

  writer_->NotifyUsesAudioInBackground();
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            test_impl_->UpdatesFaviconInBackground());
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            test_impl_->UpdatesTitleInBackground());
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            test_impl_->UsesAudioInBackground());

  writer_->NotifyLoadTimePerformanceMeasurement(base::Microseconds(202),
                                                base::Microseconds(101), 1005);
  EXPECT_EQ(1u, test_impl_->load_duration().num_datums());
  EXPECT_EQ(202.0, test_impl_->load_duration().value());
  EXPECT_EQ(1u, test_impl_->cpu_usage_estimate().num_datums());
  EXPECT_EQ(101.0, test_impl_->cpu_usage_estimate().value());

  EXPECT_EQ(1u, test_impl_->private_footprint_kb_estimate().num_datums());
  EXPECT_EQ(1005.0, test_impl_->private_footprint_kb_estimate().value());

  writer_->NotifySiteUnloaded(TabVisibility::kBackground);
}

TEST_F(SiteDataWriterTest, LoadAndBackgroundStateTransitions) {
  // There's 4 different states a tab can be in:
  //   - Unloaded + Background
  //   - Unloaded + Foreground (might not be possible in practice but this
  //     will depend on the order of the events when an unloaded background tab
  //     gets foregrounded, so it's safer to consider this state).
  //   - Loaded + Background
  //   - Loaded + Foreground
  //
  // Only one of these parameter can change at a time, so you have the following
  // possible transitions:
  //
  //   +-------------+           +-------------+
  //   |Unloaded + Bg|<--------->|Unloaded + Fg|
  //   +-------------+ 1       2 +-------------+
  //         /|\ 3                     /|\ 5
  //          |                         |
  //         \|/ 4                     \|/ 6
  //   +-------------+           +-------------+
  //   | Loaded + Bg |<--------->| Loaded + Fg |
  //   +-------------+ 7       8 +-------------+
  //
  //   - 1,2: There's nothing to do, the tab is already unloaded so |impl_|
  //       shouldn't count it as a background tab anyway.
  //   - 3: The tab gets unloaded while in background, |impl_| should be
  //       notified so it can *decrement* the counter of loaded AND backgrounded
  //       tabs.
  //   - 4: The tab gets loaded while in background, |impl_| should be notified
  //       so it can *increment* the counter of loaded AND backgrounded tabs.
  //   - 5: The tab gets unloaded while in foreground, this should theorically
  //       not happen, but if it does then |impl_| should just be notified about
  //       the unload event so it can update its last loaded timestamp.
  //   - 6: The tab gets loaded while in foreground, |impl_| should only be
  //       notified about the load event, the background state hasn't changed.
  //   - 7: A loaded foreground tab gets backgrounded, |impl_| should be
  //       notified that the tab has been background so it can *increment* the
  //       counter of loaded AND backgrounded tabs.
  //   - 8: A loaded background tab gets foregrounded, |impl_| should be
  //       notified that the tab has been background so it can *decrement* the
  //       counter of loaded AND backgrounded tabs.
  EXPECT_FALSE(TabIsLoaded());

  // Transition #4: Unloaded + Bg -> Loaded + Bg.
  writer_->NotifySiteLoaded(TabVisibility::kBackground);
  EXPECT_TRUE(TabIsLoadedAndInBackground());

  // Transition #8: Loaded + Bg -> Loaded + Fg.
  writer_->NotifySiteForegrounded(true);
  EXPECT_TRUE(TabIsLoaded());
  EXPECT_FALSE(TabIsLoadedAndInBackground());

  // Transition #5: Loaded + Fg -> Unloaded + Fg.
  writer_->NotifySiteUnloaded(TabVisibility::kForeground);
  EXPECT_FALSE(TabIsLoaded());

  // Transition #1: Unloaded + Fg -> Unloaded + Bg.
  writer_->NotifySiteBackgrounded(false);
  EXPECT_FALSE(TabIsLoaded());

  // Transition #2: Unloaded + Bg -> Unloaded + Fg.
  writer_->NotifySiteForegrounded(false);
  EXPECT_FALSE(TabIsLoaded());

  // Transition #6: Unloaded + Fg -> Loaded + Fg.
  writer_->NotifySiteLoaded(TabVisibility::kForeground);
  EXPECT_TRUE(TabIsLoaded());
  EXPECT_FALSE(TabIsLoadedAndInBackground());

  // Transition #7: Loaded + Fg -> Loaded + Bg.
  writer_->NotifySiteBackgrounded(true);
  EXPECT_TRUE(TabIsLoaded());
  EXPECT_TRUE(TabIsLoadedAndInBackground());

  // Transition #3: Loaded + Bg -> Unloaded + Bg.
  writer_->NotifySiteUnloaded(TabVisibility::kBackground);
  EXPECT_FALSE(TabIsLoaded());
  EXPECT_FALSE(TabIsLoadedAndInBackground());
}

}  // namespace performance_manager
