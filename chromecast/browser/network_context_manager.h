// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_NETWORK_CONTEXT_MANAGER_H_
#define CHROMECAST_BROWSER_NETWORK_CONTEXT_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace net {
class URLRequestContextGetter;
}

namespace network {
class NetworkContext;
class NetworkService;
}  // namespace network

namespace chromecast {

// This class manages a NetworkContext which wraps a URLRequestContextGetter
// used by Cast builds. Its main duty is to provide clients access to a
// URLLoaderFactory for the NetworkContext on any thread. A UrlLoaderFactory is
// used to create SimpleURLLoaders, which fetch content from the network.
//
// THREADING: This class may be instantiated and called on any thread. However,
// all internal state management happens on the browser's IO thread, where the
// NetworkContext lives. A URLLoaderFactoryPtr created by this class can only be
// used on the thread it was created on (see below for more details). This class
// must be deleted on the browser's IO thread.
//
// TODO(slan): Once the network service ships on Cast devices, we should get
// URLLoaderFatories directly from the network service.
class NetworkContextManager {
 public:
  // May be constructed on and live on any thread. |url_request_context_getter|
  // will be called on the IO thread and must outlive |this|.
  explicit NetworkContextManager(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);
  ~NetworkContextManager();

  // Returns an interface pointer to a URLLoaderFactory that is bound to the
  // calling thread, and can be used immediately. The request for the mojo
  // interface is asynchonously thread-hopped to the IO thread, where it is
  // bound to the URLLoaderFactory implementation. This method can be called
  // from any thread, but the interface pointer must only be used on that same
  // calling thread. The caller owns the returned interface pointer.
  network::mojom::URLLoaderFactoryPtr GetURLLoaderFactory();

  // Creates a ProxyResolvingSocketFactory asynchronously on the IO thread, and
  // binds it to |request|. This may be called from any thread.
  void GetProxyResolvingSocketFactory(
      network::mojom::ProxyResolvingSocketFactoryRequest request);

  // Get a WeakPtr to this object.
  base::WeakPtr<NetworkContextManager> GetWeakPtr() { return weak_ptr_; }

  // Creates a NetworkContextManager for testing, so that a fake NetworkService
  // can be created on the IO thread and injected into this class. Caller owns
  // the returned instance. Returns the object unwrapped so that it can be
  // destroyed on the IO thread with content::BrowserThread::DeleteOnIOThead or
  // content::BrowserThread::DeleteSoon.
  static NetworkContextManager* CreateForTest(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      std::unique_ptr<network::NetworkService> network_service);

 private:
  // Called internally by the public constructor and by CreateForTest(). Allows
  // a test NetworkService to be injected for tests.
  explicit NetworkContextManager(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      std::unique_ptr<network::NetworkService> network_service);

  // Initializes the NetworkContext. Posted to the IO thread from the ctor.
  void InitializeOnIOThread();

  // Posted to the IO thread whenever GetURLLoaderFactoryPtr() is called.
  // Guaranteed to be called after InitializeOnIoThread().
  void BindRequestOnIOThread(network::mojom::URLLoaderFactoryRequest request);

  // Create a proxy-resolving socket factory using the |network_context_| on the
  // IO thread. Posted from RequestProxyResolvingSocketFactory(). Guaranteed to
  // be called after InitializeOnIoThread().
  void GetProxyResolvingSocketFactoryOnIOThread(
      network::mojom::ProxyResolvingSocketFactoryRequest request);

  // The underlying URLRequestContextGetter. The reference may only be released
  // after the NetworkContext has been destroyed.
  const scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // An instance of NetworkService that can be hosted for testing purposes.
  // Lives on the IO thread. Must outlive |network_context_| when used.
  std::unique_ptr<network::NetworkService> network_service_for_test_;

  // This state is required to create a URLLoaderFactory for network requests.
  // These objects all live on the IO thread.
  std::unique_ptr<network::NetworkContext> network_context_;
  network::mojom::NetworkContextPtr network_context_ptr_;

  base::WeakPtr<NetworkContextManager> weak_ptr_;
  base::WeakPtrFactory<NetworkContextManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContextManager);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_NETWORK_CONTEXT_MANAGER_H_
