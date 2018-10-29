// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_DATA_USE_OBSERVER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_DATA_USE_OBSERVER_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/data_use_measurement/core/data_use.h"
#include "components/data_use_measurement/core/data_use_ascriber.h"

namespace net {
class URLRequest;
}

namespace data_reduction_proxy {

class DataReductionProxyIOData;

// Observers the page load events from DataUseAscriber and records the data
// usage per site to database.
class DataReductionProxyDataUseObserver
    : public data_use_measurement::DataUseAscriber::PageLoadObserver {
 public:
  // |data_reduction_proxy_io_data| is used to record the bytes to database.
  // |data_use_ascriber| is used to listen for events.
  // |this| is owned by |data_reduction_proxy_io_data|.
  DataReductionProxyDataUseObserver(
      DataReductionProxyIOData* data_reduction_proxy_io_data,
      data_use_measurement::DataUseAscriber* data_use_ascriber);

  ~DataReductionProxyDataUseObserver() override;

 private:
  friend class DataReductionProxyDataUseObserverTest;

  // PageLoadObserver methods.
  void OnPageLoadCommit(data_use_measurement::DataUse* data_use) override;
  void OnPageResourceLoad(const net::URLRequest& request,
                          data_use_measurement::DataUse* data_use) override;
  void OnPageDidFinishLoad(data_use_measurement::DataUse* data_use) override;
  void OnPageLoadConcluded(data_use_measurement::DataUse* data_use) override {}
  void OnNetworkBytesUpdate(const net::URLRequest& request,
                            data_use_measurement::DataUse* data_use) override {}

  // |data_reduction_proxy_io_data_| owns |this| and is destroyed only after
  // |this| is destroyed in the IO thread.
  DataReductionProxyIOData* data_reduction_proxy_io_data_;

  // Used to register for pageload events. |data_use_ascriber_| is owned by IO
  // thread globals, and is destroyed after |this|.
  data_use_measurement::DataUseAscriber* data_use_ascriber_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyDataUseObserver);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_DATA_USE_OBSERVER_H_
