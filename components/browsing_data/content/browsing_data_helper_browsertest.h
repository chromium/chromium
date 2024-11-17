// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains code shared by all browsing data browsertests.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_HELPER_BROWSERTEST_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_HELPER_BROWSERTEST_H_

#include <list>

#include "base/check.h"
#include "base/run_loop.h"

// This template can be used for the StartFetching methods of the browsing data
// helper classes. It is supposed to be instantiated with the respective
// browsing data info type.
template <typename T>
class BrowsingDataHelperCallback {
 public:
  BrowsingDataHelperCallback() = default;

  BrowsingDataHelperCallback(const BrowsingDataHelperCallback&) = delete;
  BrowsingDataHelperCallback& operator=(const BrowsingDataHelperCallback&) =
      delete;

  const std::list<T>& result() {
    run_loop_.Run();
    DCHECK(has_result_);
    return result_;
  }

  void callback(const std::list<T>& info) {
    result_ = info;
    has_result_ = true;
    run_loop_.QuitWhenIdle();
  }

 private:
  base::RunLoop run_loop_;
  bool has_result_ = false;
  std::list<T> result_;
};

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_HELPER_BROWSERTEST_H_
