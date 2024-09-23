// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_FETCH_THROTTLER_DELEGATE_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_FETCH_THROTTLER_DELEGATE_H_

namespace affiliations {

// An interface that users of AffiliationFetchThrottler need to implement to get
// notified once it is okay to issue the next network request.
class AffiliationFetchThrottlerDelegate {
 public:
  // Will be called once the throttling policy allows issuing a network request,
  // provided SignalNetworkRequestNeeded() has been called at least once since
  // the last request.
  //
  // The implementation must return true if a request was actually issued in
  // response to this call, and then call InformOfNetworkRequestComplete() once
  // the request is complete. Otherwise, the implementation must return false.
  virtual bool OnCanSendNetworkRequest() = 0;

 protected:
  virtual ~AffiliationFetchThrottlerDelegate() = default;
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_FETCH_THROTTLER_DELEGATE_H_
