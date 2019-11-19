// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_EXTENSION_URL_LOADER_FACTORY_H_
#define CHROMECAST_BROWSER_CAST_EXTENSION_URL_LOADER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
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
class CastExtensionURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  // |extension_factory| is the default extension factory that will be used if
  // the request isn't fetched from the web.
  CastExtensionURLLoaderFactory(
      content::BrowserContext* browser_context,
      std::unique_ptr<network::mojom::URLLoaderFactory> extension_factory);
  ~CastExtensionURLLoaderFactory() override;

 private:
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
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory>
                 factory_receiver) override;

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;
  extensions::ExtensionRegistry* extension_registry_;
  std::unique_ptr<network::mojom::URLLoaderFactory> extension_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> network_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastExtensionURLLoaderFactory);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_EXTENSION_URL_LOADER_FACTORY_H_
