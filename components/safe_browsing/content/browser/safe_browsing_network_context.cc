// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/safe_browsing_network_context.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "content/public/browser/browser_thread.h"
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
      bool trigger_migration,
      NetworkContextParamsFactory network_context_params_factory)
      : user_data_dir_(user_data_dir),
        trigger_migration_(trigger_migration),
        network_context_params_factory_(
            std::move(network_context_params_factory)) {}

  SharedURLLoaderFactory(const SharedURLLoaderFactory&) = delete;
  SharedURLLoaderFactory& operator=(const SharedURLLoaderFactory&) = delete;

  void Reset() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    url_loader_factory_.reset();
    network_context_.reset();
  }

  network::mojom::NetworkContext* GetNetworkContext() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    if (!network_context_ || !network_context_.is_connected()) {
      network_context_.reset();
      content::CreateNetworkContextInNetworkService(
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
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
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
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  network::mojom::URLLoaderFactory* GetURLLoaderFactory() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    if (!url_loader_factory_ || !url_loader_factory_.is_connected()) {
      url_loader_factory_.reset();
      network::mojom::URLLoaderFactoryParamsPtr params =
          network::mojom::URLLoaderFactoryParams::New();
      params->process_id = network::mojom::kBrowserProcessId;
      params->is_orb_enabled = false;
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
    TRACE_EVENT0("startup",
                 "SafeBrowsingNetworkContext::CreateNetworkContextParams");
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    network::mojom::NetworkContextParamsPtr network_context_params =
        network_context_params_factory_.Run();

    network_context_params->http_cache_enabled = false;

    network_context_params->file_paths =
        network::mojom::NetworkContextFilePaths::New();
    network_context_params->file_paths->data_directory = user_data_dir_.Append(
        base::FilePath(base::FilePath::StringType(kSafeBrowsingBaseFilename) +
                       FILE_PATH_LITERAL(" Network")));
    network_context_params->file_paths->unsandboxed_data_path = user_data_dir_;
    network_context_params->file_paths->trigger_migration = trigger_migration_;
    network_context_params->file_paths->cookie_database_name = base::FilePath(
        base::FilePath::StringType(kSafeBrowsingBaseFilename) + kCookiesFile);
    network_context_params->enable_encrypted_cookies = false;

    return network_context_params;
  }

  base::FilePath user_data_dir_;
  bool trigger_migration_;
  NetworkContextParamsFactory network_context_params_factory_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
};

SafeBrowsingNetworkContext::SafeBrowsingNetworkContext(
    const base::FilePath& user_data_dir,
    bool trigger_migration,
    NetworkContextParamsFactory network_context_params_factory) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  url_loader_factory_ = base::MakeRefCounted<SharedURLLoaderFactory>(
      user_data_dir, trigger_migration,
      std::move(network_context_params_factory));
}

SafeBrowsingNetworkContext::~SafeBrowsingNetworkContext() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

scoped_refptr<network::SharedURLLoaderFactory>
SafeBrowsingNetworkContext::GetURLLoaderFactory() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return url_loader_factory_;
}

network::mojom::NetworkContext*
SafeBrowsingNetworkContext::GetNetworkContext() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return url_loader_factory_->GetNetworkContext();
}

void SafeBrowsingNetworkContext::FlushForTesting() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  url_loader_factory_->FlushForTesting();
}

void SafeBrowsingNetworkContext::ServiceShuttingDown() {
  url_loader_factory_->Reset();
}

}  // namespace safe_browsing
