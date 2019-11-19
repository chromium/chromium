// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_COMMON_DATA_REDUCTION_PROXY_URL_LOADER_THROTTLE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_COMMON_DATA_REDUCTION_PROXY_URL_LOADER_THROTTLE_H_

#include <vector>

#include "base/sequence_checker.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_throttle_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace data_reduction_proxy {

struct DataReductionProxyTypeInfo;

// Handles Data Reduction Proxy logic that needs to be applied to each request.
//
// This includes:
//   * Setting request headers for the data reduction proxy.
//   * Processing response headers from a data reduction proxy.
//   * Restarting the URL loader in order to use a different proxy.
//   * Marking data reduction proxies to be bypassed for future requests.
class DataReductionProxyURLLoaderThrottle
    : public blink::URLLoaderThrottle,
      public DataReductionProxyThrottleConfigCheckedObserver {
 public:
  // |manager| is shared between all the DRP Throttles.
  DataReductionProxyURLLoaderThrottle(
      const net::HttpRequestHeaders& post_cache_headers,
      DataReductionProxyThrottleManager* manager);
  ~DataReductionProxyURLLoaderThrottle() override;

  // blink::URLLoaderThrottle:
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_request_headers,
      net::HttpRequestHeaders* modified_request_headers) override;
  void BeforeWillProcessResponse(
      const GURL& response_url,
      const network::mojom::URLResponseHead& response_head,
      bool* defer) override;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;
  void WillOnCompleteWithError(const network::URLLoaderCompletionStatus& status,
                               bool* defer) override;

  // DataReductionProxyThrottleConfigCheckedObserver:
  void OnThrottleConfigChanged(
      mojom::DataReductionProxyThrottleConfigPtr config) override;
  void OnThrottleManagerDestroyed(
      DataReductionProxyThrottleManager* manager) override;

 private:
  // As the throttle instance is being moved to another sequence, this
  // functions arranges for mojom::DataReductionProxy interactions to happen
  // through this throttle's private mojo pipes.
  void SetUpPrivateMojoPipes();

  // Retry the request bypassing proxies or falling back to next proxy based on
  // |net_error| and the response headers.
  void MaybeRetry(const net::ProxyServer& proxy_server,
                  const net::HttpResponseHeaders* headers,
                  net::Error net_error,
                  bool* defer);
  // Marks |bad_proxies| to be bypassed for |bypass_duration|. Once that action
  // has completed will call OnMarkProxiesAsBadComplete().
  void MarkProxiesAsBad(const std::vector<net::ProxyServer>& bad_proxies,
                        base::TimeDelta bypass_duration);
  void OnMarkProxiesAsBadComplete();

  // Tells |delegate_| to restart the URL loader if |pending_restart_| was set.
  void DoPendingRestart();

  base::Optional<DataReductionProxyTypeInfo> FindConfiguredDataReductionProxy(
      const net::ProxyServer& proxy_server) const;

  net::HttpRequestHeaders post_cache_headers_;

  // List of URLs in the redirect chain. |.front()| is the original URL
  // requested, and |.back()| is the latest URL that was redirected to.
  std::vector<GURL> url_chain_;
  std::string request_method_;

  // The throttle must be initialized with a valid manager, but can later be
  // disassociated from it if the manager is destroyed earlier or if the
  // throttle is moved to a different sequence.
  DataReductionProxyThrottleManager* manager_ = nullptr;

  // Throttles that run on the same sequence as the manager share the manager's
  // mojo pipes. In this case, |data_reduction_proxy_| routes calls through the
  // manager's connection to mojom::DataReductionProxy, and
  // mojom::DataReductionProxyThrottleConfigObserver events received by the
  // manager are forwarded to the same-sequence throttles.
  mojom::DataReductionProxy* data_reduction_proxy_;

  // Throttles that run on different sequences need "private" mojo pipes.
  mojo::PendingRemote<mojom::DataReductionProxy>
      private_data_reduction_proxy_remote_;
  mojo::Remote<mojom::DataReductionProxy> private_data_reduction_proxy_;
  mojo::Receiver<mojom::DataReductionProxyThrottleConfigObserver>
      private_config_observer_receiver_{this};

  // The last seen config values.
  std::vector<DataReductionProxyServer> proxies_for_http_;

  // |pending_restart_| is set to true if the URL loader needs to be restarted
  // using |pending_restart_load_flags_|.
  int pending_restart_load_flags_ = 0;
  bool pending_restart_ = false;

  // Set to true while waiting for OnMarkProxiesAsBadComplete to run.
  bool waiting_for_mark_proxies_ = false;

  // Whether this throttle is intercepting a main frame request.
  bool is_main_frame_ = false;

  // The final load flags used to complete the request.
  int final_load_flags_ = 0;

  // True if BeforeWillProcessResponse has been called.
  bool before_will_process_response_received_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DataReductionProxyURLLoaderThrottle> weak_factory_{this};
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_COMMON_DATA_REDUCTION_PROXY_URL_LOADER_THROTTLE_H_
