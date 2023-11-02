// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NET_HTTP_BRIDGE_H_
#define COMPONENTS_SYNC_ENGINE_NET_HTTP_BRIDGE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/sync/engine/net/http_post_provider.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

class HttpBridgeTest;

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace syncer {

// A bridge between the syncer and Chromium HTTP layers.
// Provides a way for the sync backend to use Chromium directly for HTTP
// requests rather than depending on a third party provider (e.g libcurl).
// This is a one-time use bridge. Create one for each request you want to make.
class HttpBridge : public HttpPostProvider {
 public:
  HttpBridge(const std::string& user_agent,
             std::unique_ptr<network::PendingSharedURLLoaderFactory>
                 pending_url_loader_factory);

  HttpBridge(const HttpBridge&) = delete;
  HttpBridge& operator=(const HttpBridge&) = delete;

  // HttpPostProvider implementation.
  void SetExtraRequestHeaders(const char* headers) override;
  void SetURL(const GURL& url) override;
  void SetPostPayload(const char* content_type,
                      int content_length,
                      const char* content) override;
  void SetAllowBatching(bool allow_batching) override;
  bool MakeSynchronousPost(int* net_error_code, int* http_status_code) override;
  void Abort() override;

  // WARNING: these response content methods are used to extract plain old data
  // and not null terminated strings, so you should make sure you have read
  // GetResponseContentLength() characters when using GetResponseContent. e.g
  // string r(b->GetResponseContent(), b->GetResponseContentLength()).
  int GetResponseContentLength() const override;
  const char* GetResponseContent() const override;
  const std::string GetResponseHeaderValue(
      const std::string& name) const override;

  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);
  void OnURLLoadUploadProgress(uint64_t position, uint64_t total);

  static void SetIOCapableTaskRunnerForTest(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 protected:
  ~HttpBridge() override;

  // Protected virtual so the unit test can override to shunt network requests.
  virtual void MakeAsynchronousPost();

  void set_url_loader_factory_for_testing(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
    url_loader_factory_ = url_loader_factory;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(SyncHttpBridgeTest,
                           AbortAndReleaseBeforeFetchComplete);
  // Test is disabled on Android.
  FRIEND_TEST_ALL_PREFIXES(DISABLED_SyncHttpBridgeTest,
                           AbortAndReleaseBeforeFetchComplete);
  friend class ShuntedHttpBridge;
  friend class ::HttpBridgeTest;

  // Called on the IO loop to issue the network request. The extra level
  // of indirection is so that the unit test can override this behavior but we
  // still have a function to statically pass to PostTask.
  void CallMakeAsynchronousPost() { MakeAsynchronousPost(); }

  // Actual implementation of the load complete callback. Called by tests too.
  void OnURLLoadCompleteInternal(int http_status_code,
                                 int net_error_code,
                                 const GURL& final_url,
                                 std::unique_ptr<std::string> response_body);

  // Used to destroy a fetcher when the bridge is Abort()ed, to ensure that
  // a reference to |this| is held while flushing any pending fetch completion
  // callbacks coming from the IO thread en route to finally destroying the
  // fetcher.
  void DestroyURLLoaderOnIOThread(
      std::unique_ptr<network::SimpleURLLoader> loader,
      std::unique_ptr<base::DelayTimer> loader_timer);

  // Helper method to abort the request if we timed out.
  void OnURLLoadTimedOut();

  // Used to check whether a method runs on the thread that we were created on.
  // This is the thread that will block on MakeSynchronousPost while the IO
  // thread fetches data from the network.
  // This should be the main syncer thread (SyncerThread) which is what blocks
  // on network IO through curl_easy_perform.
  base::ThreadChecker thread_checker_;

  // The user agent for all requests.
  const std::string user_agent_;

  // The URL to POST to.
  GURL url_for_request_;

  // POST payload information.
  std::string content_type_;
  std::string request_content_;
  std::string extra_headers_;

  // When true `fetch_state_.url_loader` is configured so that it can be
  // batched in the network layer. See the comment in
  // network::SimpleURLLoader::SetAllowBatching().
  bool allow_batching_ = false;

  // A waitable event we use to provide blocking semantics to
  // MakeSynchronousPost. We block created_on_loop_ while the IO loop fetches
  // network request.
  base::WaitableEvent http_post_completed_;

  struct URLFetchState {
    URLFetchState();
    ~URLFetchState();
    // Our hook into the network layer is a URLFetcher. USED ONLY ON THE IO
    // LOOP, so we can block created_on_loop_ while the fetch is in progress.
    // NOTE: This is not a unique_ptr for a reason. It must be deleted on the
    // same thread that created it, which isn't the same thread |this| gets
    // deleted on. We must manually delete url_poster_ on the IO loop.
    std::unique_ptr<network::SimpleURLLoader> url_loader;

    // Start and finish time of request. Set immediately before sending
    // request and after receiving response.
    base::Time start_time;
    base::Time end_time;

    // Used to support 'Abort' functionality.
    bool aborted;

    // Cached response data.
    bool request_completed;
    bool request_succeeded;
    int http_status_code;
    int net_error_code;
    std::string response_content;
    scoped_refptr<net::HttpResponseHeaders> response_headers;

    // Timer to ensure http requests aren't stalled. Reset every time upload or
    // download progress is made.
    std::unique_ptr<base::DelayTimer> http_request_timeout_timer;
  };

  // This lock synchronizes use of state involved in the flow to load a URL
  // using URLLoader, including |fetch_state_| on any thread, for example,
  // this flow needs to be synchronized to gracefully
  // clean up URLFetcher and return appropriate values in |error_code|.
  //
  // TODO(crbug.com/844968): Check whether we can get rid of |fetch_state_lock_|
  // altogether after the migration to SimpleURLLoader.
  mutable base::Lock fetch_state_lock_;
  URLFetchState fetch_state_;

  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  const scoped_refptr<base::SequencedTaskRunner> network_task_runner_;
};

class HttpBridgeFactory : public HttpPostProviderFactory {
 public:
  HttpBridgeFactory(const std::string& user_agent,
                    std::unique_ptr<network::PendingSharedURLLoaderFactory>
                        pending_url_loader_factory);

  HttpBridgeFactory(const HttpBridgeFactory&) = delete;
  HttpBridgeFactory& operator=(const HttpBridgeFactory&) = delete;

  ~HttpBridgeFactory() override;

  // HttpPostProviderFactory:
  scoped_refptr<HttpPostProvider> Create() override;

 private:
  // The user agent to use in all requests.
  const std::string user_agent_;

  // The URL loader factory used for making all requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  //  namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_NET_HTTP_BRIDGE_H_
