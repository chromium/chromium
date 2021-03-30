// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_network_context.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/common/thread_utils.h"
#include "content/public/browser/network_context_client_base.h"
#include "content/public/browser/network_service_instance.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/net_buildflags.h"
#include "services/network/network_context.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace safe_browsing {

class SafeBrowsingNetworkContext::SharedURLLoaderFactory
    : public network::SharedURLLoaderFactory {
 public:
  SharedURLLoaderFactory(
      const base::FilePath& user_data_dir,
      NetworkContextParamsFactory network_context_params_factory)
      : user_data_dir_(user_data_dir),
        network_context_params_factory_(
            std::move(network_context_params_factory)) {}

  void Reset() {
    DCHECK(CurrentlyOnThread(ThreadID::UI));
    url_loader_factory_.reset();
    network_context_.reset();
  }

  network::mojom::NetworkContext* GetNetworkContext() {
    DCHECK(CurrentlyOnThread(ThreadID::UI));
    if (!network_context_ || !network_context_.is_connected()) {
      network_context_.reset();
      content::GetNetworkService()->CreateNetworkContext(
          network_context_.BindNewPipeAndPassReceiver(),
          CreateNetworkContextParams());

      mojo::PendingRemote<network::mojom::NetworkContextClient> client_remote;
      mojo::MakeSelfOwnedReceiver(
          std::make_unique<content::NetworkContextClientBase>(),
          client_remote.InitWithNewPipeAndPassReceiver());
      network_context_->SetClient(std::move(client_remote));
    }
    return network_context_.get();
  }

  void FlushForTesting() {
    if (network_context_)
      network_context_.FlushForTesting();
    if (url_loader_factory_)
      url_loader_factory_.FlushForTesting();
  }

 protected:
  // network::URLLoaderFactory implementation:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    DCHECK(CurrentlyOnThread(ThreadID::UI));
    GetURLLoaderFactory()->CreateLoaderAndStart(
        std::move(loader), request_id, options, request, std::move(client),
        traffic_annotation);
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    GetURLLoaderFactory()->Clone(std::move(receiver));
  }

  // network::SharedURLLoaderFactory implementation:
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    NOTREACHED();
    return nullptr;
  }

  network::mojom::URLLoaderFactory* GetURLLoaderFactory() {
    DCHECK(CurrentlyOnThread(ThreadID::UI));
    if (!url_loader_factory_ || !url_loader_factory_.is_connected()) {
      url_loader_factory_.reset();
      network::mojom::URLLoaderFactoryParamsPtr params =
          network::mojom::URLLoaderFactoryParams::New();
      params->process_id = network::mojom::kBrowserProcessId;
      params->is_corb_enabled = false;
      params->is_trusted = true;
      GetNetworkContext()->CreateURLLoaderFactory(
          url_loader_factory_.BindNewPipeAndPassReceiver(), std::move(params));
    }
    return url_loader_factory_.get();
  }

 private:
  friend class base::RefCounted<SharedURLLoaderFactory>;
  ~SharedURLLoaderFactory() override = default;

  network::mojom::NetworkContextParamsPtr CreateNetworkContextParams() {
    DCHECK(CurrentlyOnThread(ThreadID::UI));
    network::mojom::NetworkContextParamsPtr network_context_params =
        network_context_params_factory_.Run();

    network_context_params->context_name = std::string("safe_browsing");

    network_context_params->http_cache_enabled = false;

    // These are needed for PAC scripts that use FTP URLs.
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
    network_context_params->enable_ftp_url_support = true;
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

    base::FilePath cookie_path = user_data_dir_.Append(
        base::FilePath::StringType(kSafeBrowsingBaseFilename) + kCookiesFile);
    network_context_params->cookie_path = cookie_path;
    network_context_params->enable_encrypted_cookies = false;

    return network_context_params;
  }

  base::FilePath user_data_dir_;
  NetworkContextParamsFactory network_context_params_factory_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(SharedURLLoaderFactory);
};

SafeBrowsingNetworkContext::SafeBrowsingNetworkContext(
    const base::FilePath& user_data_dir,
    NetworkContextParamsFactory network_context_params_factory) {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
  url_loader_factory_ = base::MakeRefCounted<SharedURLLoaderFactory>(
      user_data_dir, std::move(network_context_params_factory));
}

SafeBrowsingNetworkContext::~SafeBrowsingNetworkContext() {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
}

scoped_refptr<network::SharedURLLoaderFactory>
SafeBrowsingNetworkContext::GetURLLoaderFactory() {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
  return url_loader_factory_;
}

network::mojom::NetworkContext*
SafeBrowsingNetworkContext::GetNetworkContext() {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
  return url_loader_factory_->GetNetworkContext();
}

void SafeBrowsingNetworkContext::FlushForTesting() {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
  url_loader_factory_->FlushForTesting();
}

void SafeBrowsingNetworkContext::ServiceShuttingDown() {
  url_loader_factory_->Reset();
}

}  // namespace safe_browsing
