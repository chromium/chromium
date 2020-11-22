// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_EXTENSION_URL_LOADER_FACTORY_H_
#define CHROMECAST_BROWSER_CAST_EXTENSION_URL_LOADER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/non_network_url_loader_factory_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class ExtensionRegistry;
}

namespace chromecast {
namespace shell {

// URLLoaderFactory that creates URLLoader instances for URLs with the
// extension scheme. Cast uses its own factory that resues the extensions
// URLLoader implementation because Cast sometimes loads extension resources
// from the web.
class CastExtensionURLLoaderFactory
    : public content::NonNetworkURLLoaderFactoryBase {
 public:
  // Returns mojo::PendingRemote to a newly constructed
  // CastExtensionURLLoaderFactory.  The factory is self-owned - it will delete
  // itself once there are no more receivers (including the receiver associated
  // with the returned mojo::PendingRemote and the receivers bound by the Clone
  // method).
  //
  // |extension_factory| is the default extension factory that will be used if
  // the request isn't fetched from the web.
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create(
      content::BrowserContext* browser_context,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> extension_factory);

  static void EnsureShutdownNotifierFactoryBuilt();

 private:
  ~CastExtensionURLLoaderFactory() override;

  // |extension_factory| is the default extension factory that will be used if
  // the request isn't fetched from the web.
  CastExtensionURLLoaderFactory(
      content::BrowserContext* browser_context,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> extension_factory,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

  void OnBrowserContextDestroyed();

  class BrowserContextShutdownNotifierFactory
      : public BrowserContextKeyedServiceShutdownNotifierFactory {
   public:
    static BrowserContextShutdownNotifierFactory* GetInstance();

    // No copying.
    BrowserContextShutdownNotifierFactory(
        const BrowserContextShutdownNotifierFactory&) = delete;
    BrowserContextShutdownNotifierFactory& operator=(
        const BrowserContextShutdownNotifierFactory&) = delete;

   private:
    friend class base::NoDestructor<BrowserContextShutdownNotifierFactory>;
    BrowserContextShutdownNotifierFactory();
  };

  extensions::ExtensionRegistry* extension_registry_;
  mojo::Remote<network::mojom::URLLoaderFactory> extension_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> network_factory_;

  base::CallbackListSubscription browser_context_shutdown_subscription_;

  DISALLOW_COPY_AND_ASSIGN(CastExtensionURLLoaderFactory);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_EXTENSION_URL_LOADER_FACTORY_H_
