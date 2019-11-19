// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_NETWORK_CONTEXTS_H_
#define CHROMECAST_BROWSER_CAST_NETWORK_CONTEXTS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/proxy_config.mojom.h"

class PrefProxyConfigTracker;

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace network {
namespace mojom {
class NetworkContext;
class URLLoaderFactory;
}  // namespace mojom
class SharedURLLoaderFactory;
}  // namespace network

namespace chromecast {
namespace shell {

// This class owns the NetworkContext used for the system and for configuring it
// along with the BrowserContext's NetworkContext.
// It will create and configure its own NetworkContext for the system, and
// create the BrowserContext's main StoragePartition's NetworkContext.
// It lives on the UI thread.
class CastNetworkContexts : public net::ProxyConfigService::Observer,
                            public network::mojom::ProxyConfigPollerClient {
 public:
  explicit CastNetworkContexts(
      std::vector<std::string> cors_exempt_headers_list);
  ~CastNetworkContexts() override;

  // Returns the System NetworkContext. Does any initialization of the
  // NetworkService that may be needed when first called.
  network::mojom::NetworkContext* GetSystemContext();

  // Returns a URLLoaderFactory owned by the CastNetworkContexts that is
  // backed by the system NetworkContext. Allows sharing of the
  // URLLoaderFactory. Prefer this to creating a new one.  Call Clone() on the
  // value returned by this method to get a URLLoaderFactory that can be used on
  // other threads.
  network::mojom::URLLoaderFactory* GetSystemURLLoaderFactory();

  // Returns a SharedURLLoaderFactory that is backed by the system
  // NetworkContext.
  scoped_refptr<network::SharedURLLoaderFactory>
  GetSystemSharedURLLoaderFactory();

  // Called when content creates a NetworkService. Creates the
  // system NetworkContext, if the network service is enabled.
  void OnNetworkServiceCreated(network::mojom::NetworkService* network_service);

  mojo::Remote<network::mojom::NetworkContext> CreateNetworkContext(
      content::BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path);

  // Called when the locale has changed.
  void OnLocaleUpdate();

  // Called on shutdown of the PrefService.
  void OnPrefServiceShutdown();

 private:
  class URLLoaderFactoryForSystem;

  // Returns default set of parameters for configuring the network service.
  network::mojom::NetworkContextParamsPtr CreateDefaultNetworkContextParams();

  // Creates parameters for the system NetworkContext. May only be called once,
  // since it initializes some class members.
  network::mojom::NetworkContextParamsPtr CreateSystemNetworkContextParams();

  // Populates proxy-related fields of |network_context_params|. Updated
  // ProxyConfigs will be sent to a NetworkContext created with those params
  // whenever the configuration changes. Can be called more than once to inform
  // multiple NetworkContexts of proxy changes.
  void AddProxyToNetworkContextParams(
      network::mojom::NetworkContextParams* network_context_params);

  // net::ProxyConfigService::Observer implementation:
  void OnProxyConfigChanged(
      const net::ProxyConfigWithAnnotation& config,
      net::ProxyConfigService::ConfigAvailability availability) override;

  // network::mojom::ProxyConfigPollerClient implementation:
  void OnLazyProxyConfigPoll() override;

  const std::vector<std::string> cors_exempt_headers_list_;

  // The system NetworkContext.
  mojo::Remote<network::mojom::NetworkContext> system_network_context_;

  // URLLoaderFactory backed by the NetworkContext returned by
  // GetSystemContext(), so consumers don't all need to create their own
  // factory.
  scoped_refptr<URLLoaderFactoryForSystem> system_shared_url_loader_factory_;
  mojo::Remote<network::mojom::URLLoaderFactory> system_url_loader_factory_;

  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  // Monitors prefs related to proxy configuration.
  std::unique_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_impl_;

  mojo::ReceiverSet<network::mojom::ProxyConfigPollerClient>
      poller_receiver_set_;
  mojo::RemoteSet<network::mojom::ProxyConfigClient> proxy_config_client_set_;

  DISALLOW_COPY_AND_ASSIGN(CastNetworkContexts);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_NETWORK_CONTEXTS_H_
