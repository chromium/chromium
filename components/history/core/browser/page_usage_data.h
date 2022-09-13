// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_PAGE_USAGE_DATA_H__
#define COMPONENTS_HISTORY_CORE_BROWSER_PAGE_USAGE_DATA_H__

#include <string>

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

  double score_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_PAGE_USAGE_DATA_H__
