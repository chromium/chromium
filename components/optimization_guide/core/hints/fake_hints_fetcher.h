// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_FAKE_HINTS_FETCHER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_FAKE_HINTS_FETCHER_H_

namespace optimization_guide {

// Selects fake behavior for HintsFetcher
// TODO: crbug.com/421262905 - Move the logic that implements this from
// chrome/browser/optimization_guide/hints_fetcher_browsertest.cc
enum class HintsFetcherRemoteResponseType {
  kSuccessful = 0,
  kUnsuccessful = 1,
  kMalformed = 2,
  kHung = 3,
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_FAKE_HINTS_FETCHER_H_
