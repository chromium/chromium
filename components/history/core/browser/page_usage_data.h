// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_PAGE_USAGE_DATA_H__
#define COMPONENTS_HISTORY_CORE_BROWSER_PAGE_USAGE_DATA_H__

#include <string>

#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "url/gurl.h"

namespace history {

/////////////////////////////////////////////////////////////////////////////
//
// PageUsageData
//
// A per domain usage data structure to compute and manage most visited
// pages.
//
// See QueryPageUsageSince()
//
/////////////////////////////////////////////////////////////////////////////
class PageUsageData {
 public:
  explicit PageUsageData(SegmentID id);

  virtual ~PageUsageData();

  // Return the url ID
  SegmentID GetID() const { return id_; }

  void SetURL(const GURL& url) {
    url_ = url;
  }

  const GURL& GetURL() const {
    return url_;
  }

  void SetTitle(const std::u16string& s) { title_ = s; }

  const std::u16string& GetTitle() const { return title_; }

  // Return the segment visit count.
  void SetVisitCount(int visit_count) { visit_count_ = visit_count; }

  int GetVisitCount() const { return visit_count_; }

  void SetLastVisitTimeslot(base::Time last_visit_timeslot) {
    last_visit_timeslot_ = last_visit_timeslot;
  }

  // Return the day of the last visit to the segment.
  base::Time GetLastVisitTimeslot() const { return last_visit_timeslot_; }

  void SetScore(double v) {
    score_ = v;
  }

  double GetScore() const {
    return score_;
  }

 private:
  SegmentID id_;
  GURL url_;
  std::u16string title_;
  int visit_count_{0};
  base::Time last_visit_timeslot_{base::Time::Min()};
  double score_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_PAGE_USAGE_DATA_H__
