// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_METRICS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_METRICS_H_

#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/metrics_util.h"
#include "base/optional.h"
#include "ui/compositor/throughput_tracker.h"
#endif

namespace views {
class Widget;
}

// Records metrics around the number and duration of hover cards and hover card
// images that the user sees.
class TabHoverCardMetrics {
 public:
  // Proxy for the hover card controller that can be stubbed out during tests.
  class Delegate {
   public:
    virtual ~Delegate();

    // Returns the number of tabs. Provided by the delegate so we don't need a
    // live browser to get the tab count in tests.
    virtual size_t GetTabCount() const = 0;

    // Returns true if preview images are enabled.
    virtual bool ArePreviewsEnabled() const = 0;

    // Returns true if the current hover card is visible and displaying a valid
    // preview image.
    virtual bool HasPreviewImage() const = 0;

    // Returns the hover card widget, or nullptr if none. Can be stubbed to
    // return nullptr for tests (low-level performance metrics may not be
    // recorded).
    virtual views::Widget* GetHoverCardWidget() = 0;
  };

  // Use an immutable, opaque pointer to tabs because these pointers could
  // become stale so we should not attempt to dereference them.
  using TabHandle = const void*;

  // Histogram names:
  static const char kHistogramPrefixHoverCardsSeenBeforeSelection[];
  static const char kHistogramPrefixPreviewsSeenBeforeSelection[];
  static const char kHistogramPrefixTabHoverCardTime[];
  static const char kHistogramPrefixTabPreviewTime[];
  static const char kHistogramPrefixLastTabHoverCardTime[];
  static const char kHistogramPrefixLastTabPreviewTime[];
  static const char kHistogramTimeSinceLastVisible[];

  explicit TabHoverCardMetrics(Delegate* delegate);
  TabHoverCardMetrics(const TabHoverCardMetrics& other) = delete;
  ~TabHoverCardMetrics();
  void operator=(const TabHoverCardMetrics& other) = delete;

  void TabSelectionChanged();
  void InitialCardBeingShown();
  void CardFadingIn();
  void CardWillBeHidden();
  void CardFadingOut();
  void CardFadeComplete();
  void CardFadeCanceled();

  // Notes that a card becomes fully visible or lands on|tab|. Set
  // |has_thumbnail| to true if the thumbnail for the tab is already loaded.
  void CardFullyVisibleOnTab(TabHandle tab, bool is_active);

  // Note that an image was shown for |tab|.
  void ImageLoadedForTab(TabHandle tab);

  // Records the number of cards seen before a mouse selection. Should be called
  // when the mouse is clicked on a tab, but before the selection is committed.
  void TabSelectedViaMouse(TabHandle tab);

  int cards_seen_count() const { return cards_seen_count_; }
  int images_seen_count() const { return images_seen_count_; }

  // Replacement for tab_count_metrics::BucketForTabCount() which reduces the
  // number of buckets, especially at the low end.
  static size_t GetBucketForTabCount(size_t tab_count);

  // Replacement for tab_count_metrics::HistogramName() which reduces the
  // number of buckets, especially at the low end.
  static std::string GetBucketHistogramName(const std::string& prefix,
                                            size_t bucket);

 private:
  // Clears all data for a fresh set of metrics.
  void Reset();

  void RecordTabTimeMetrics();

  int cards_seen_count_ = 0;
  int images_seen_count_ = 0;
  base::TimeTicks last_tab_time_;
  base::TimeTicks last_image_time_;

  // Timestamp of the last time a hover card was visible, recorded before it is
  // hidden. This is used for metrics.
  base::TimeTicks last_visible_timestamp_;

  // Keep this as an opaque pointer to avoid the temptation to dereference it;
  // there's a chance it could be dead.
  TabHandle last_tab_ = TabHandle();

  // The last tab we have times for. This helps us know after a fade-out caused
  // by a tab selection which tab |last_tab_time| and |last_image_time| are for.
  TabHandle times_for_last_tab_ = TabHandle();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::Optional<ui::ThroughputTracker> throughput_tracker_;
#endif

  // TOOD(dfried): in future, change this to a delegate object in order to be
  // able to test it in isolation.
  Delegate* const delegate_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_METRICS_H_
