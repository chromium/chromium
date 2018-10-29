// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_ASCRIBER_H_
#define COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_ASCRIBER_H_

#include <stdint.h>

#include <memory>

#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/data_use_measurement/core/data_use_measurement.h"
#include "url/gurl.h"

namespace net {
class NetworkDelegate;
class URLRequest;
}

namespace data_use_measurement {

class DataUse;
class DataUseRecorder;
class URLRequestClassifier;

// Abstract class that manages instances of DataUseRecorder and maps
// a URLRequest instance to its appropriate DataUseRecorder. An embedder
// should provide an override if it is interested in tracking data usage. Data
// use from all URLRequests mapped to the same DataUseRecorder will be grouped
// together and reported as a single use.
class DataUseAscriber {
 public:
  // Provides the interface for observing data use of a pageload.
  class PageLoadObserver {
   public:
    virtual ~PageLoadObserver() {}

    // The page load committed. |data_use| contains the currently committed URL,
    // and the network data used by the page so far.
    virtual void OnPageLoadCommit(DataUse* data_use) = 0;

    // A resource of a page loaded. This includes main frame, sub frame
    // resource, subresource. |data_use| contains the network data used by the
    // page so far. URL in |data_use| may not be available until OnCommit.
    virtual void OnPageResourceLoad(const net::URLRequest& request,
                                    DataUse* data_use) = 0;

    // The DidFinishLoad event occurred for the main frame. That is, the page
    // load is nominally done (however, the page can still issue more network
    // requests between this event and |OnPageLoadConcluded|.
    virtual void OnPageDidFinishLoad(DataUse* data_use) = 0;

    // The page load completed. This is when the tab is closed or another
    // navigation starts due to omnibox search, link clicks, page reload, etc.
    virtual void OnPageLoadConcluded(DataUse* data_use) = 0;

    // Called whenever a request uses any amount of network data. |request| is
    // the corresponding request that used data. |data_use| contains the network
    // data used by the page so far. URL in |data_use| may not be available
    // until OnCommit.
    virtual void OnNetworkBytesUpdate(const net::URLRequest& request,
                                      DataUse* data_use) = 0;
  };

  DataUseAscriber();
  virtual ~DataUseAscriber();

  // Creates a network delegate that will be used to track data use.
  virtual std::unique_ptr<net::NetworkDelegate> CreateNetworkDelegate(
      std::unique_ptr<net::NetworkDelegate> wrapped_network_delegate) = 0;

  // Returns the DataUseRecorder to which data usage for the given URL should
  // be ascribed. If no existing DataUseRecorder exists, a new one will be
  // created.
  virtual DataUseRecorder* GetOrCreateDataUseRecorder(
      net::URLRequest* request) = 0;

  // Returns the existing DataUseRecorder to which data usage for the given URL
  // should be ascribed.
  virtual DataUseRecorder* GetDataUseRecorder(
      const net::URLRequest& request) = 0;

  // Returns a URLRequestClassifier that can classify requests for metrics
  // recording.
  virtual std::unique_ptr<URLRequestClassifier> CreateURLRequestClassifier()
      const = 0;

  // Observers should be added or removed in IO thread. The notifications will
  // be called in the same thread.
  void AddObserver(PageLoadObserver* observer) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    observers_.AddObserver(observer);
  }

  void RemoveObserver(PageLoadObserver* observer) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    observers_.RemoveObserver(observer);
  }

  // Methods called by DataUseNetworkDelegate to propagate data use information:
  // OnBeforeUrlRequest may be called twice. e.g., in case of redirects.
  virtual void OnBeforeUrlRequest(net::URLRequest* request);
  virtual void OnNetworkBytesSent(net::URLRequest* request, int64_t bytes_sent);
  virtual void OnNetworkBytesReceived(net::URLRequest* request,
                                      int64_t bytes_received);
  virtual void OnUrlRequestCompleted(net::URLRequest* request, bool started);
  virtual void OnUrlRequestDestroyed(net::URLRequest* request);

  // Disables data use ascriber.
  virtual void DisableAscriber();

 protected:
  base::ObserverList<PageLoadObserver>::Unchecked observers_;

 private:
  THREAD_CHECKER(thread_checker_);
};

}  // namespace data_use_measurement

#endif  // COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_ASCRIBER_H_
