// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/browser/safe_browsing_network_context.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "components/safe_browsing/common/safebrowsing_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "net/net_buildflags.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace safe_browsing {

class SafeBrowsingNetworkContext::SharedURLLoaderFactory
    : public network::SharedURLLoaderFactory {
 public:
  SharedURLLoaderFactory(
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      const base::FilePath& user_data_dir,
      NetworkContextParamsFactory network_context_params_factory)
      : request_context_getter_(request_context_getter),
        user_data_dir_(user_data_dir),
        network_context_params_factory_(
            std::move(network_context_params_factory)) {}

  void Reset() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    url_loader_factory_.reset();
    network_context_.reset();
    request_context_getter_ = nullptr;
    if (internal_state_) {
      base::PostTaskWithTraits(
          FROM_HERE, {content::BrowserThread::IO},
          base::BindOnce(&InternalState::Reset, internal_state_));
    }
  }

  network::mojom::NetworkContext* GetNetworkContext() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    if (!network_context_ || network_context_.encountered_error()) {
      if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
        content::GetNetworkService()->CreateNetworkContext(
            MakeRequest(&network_context_), CreateNetworkContextParams());
      } else {
        internal_state_ = base::MakeRefCounted<InternalState>();
        internal_state_->Initialize(request_context_getter_,
                                    MakeRequest(&network_context_));
      }
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
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    GetURLLoaderFactory()->CreateLoaderAndStart(
        std::move(loader), routing_id, request_id, options, request,
        std::move(client), traffic_annotation);
  }

  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    GetURLLoaderFactory()->Clone(std::move(request));
  }

  // network::SharedURLLoaderFactory implementation:
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override {
    NOTREACHED();
    return nullptr;
  }

  network::mojom::URLLoaderFactory* GetURLLoaderFactory() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    if (!url_loader_factory_ || url_loader_factory_.encountered_error()) {
      network::mojom::URLLoaderFactoryParamsPtr params =
          network::mojom::URLLoaderFactoryParams::New();
      params->process_id = network::mojom::kBrowserProcessId;
      params->is_corb_enabled = false;
      GetNetworkContext()->CreateURLLoaderFactory(
          MakeRequest(&url_loader_factory_), std::move(params));
    }
    return url_loader_factory_.get();
  }

 private:
  // This class holds on to the network::NetworkContext object on the IO thread.
  class InternalState : public base::RefCountedThreadSafe<InternalState> {
   public:
    InternalState() = default;

    void Initialize(
        scoped_refptr<net::URLRequestContextGetter> request_context_getter,
        network::mojom::NetworkContextRequest network_context_request) {
      base::PostTaskWithTraits(
          FROM_HERE, {content::BrowserThread::IO},
          base::BindOnce(&InternalState::InitOnIO, this, request_context_getter,
                         std::move(network_context_request)));
    }

    void Reset() {
      DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
      network_context_impl_.reset();
      request_context_getter_ = nullptr;
    }

   private:
    friend class base::RefCountedThreadSafe<InternalState>;
    virtual ~InternalState() {}

    void InitOnIO(
        scoped_refptr<net::URLRequestContextGetter> request_context_getter,
        network::mojom::NetworkContextRequest network_context_request) {
      request_context_getter_ = std::move(request_context_getter);
      network_context_impl_ = std::make_unique<network::NetworkContext>(
          content::GetNetworkServiceImpl(), std::move(network_context_request),
          request_context_getter_->GetURLRequestContext());
    }

    scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
    std::unique_ptr<network::NetworkContext> network_context_impl_;

    DISALLOW_COPY_AND_ASSIGN(InternalState);
  };

  friend class base::RefCounted<SharedURLLoaderFactory>;
  ~SharedURLLoaderFactory() override = default;

  network::mojom::NetworkContextParamsPtr CreateNetworkContextParams() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    network::mojom::NetworkContextParamsPtr network_context_params =
        network_context_params_factory_.Run();

    network_context_params->context_name = std::string("safe_browsing");

    network_context_params->http_cache_enabled = false;

    // These are needed for PAC scripts that use file, data or FTP URLs.
    network_context_params->enable_data_url_support = true;
    network_context_params->enable_file_url_support = true;
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
    network_context_params->enable_ftp_url_support = true;
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

    base::FilePath cookie_path = user_data_dir_.Append(
        base::FilePath::StringType(kSafeBrowsingBaseFilename) + kCookiesFile);
    network_context_params->cookie_path = cookie_path;
    network_context_params->enable_encrypted_cookies = false;

    base::FilePath channel_id_path = user_data_dir_.Append(
        base::FilePath::StringType(kSafeBrowsingBaseFilename) + kChannelIDFile);
    network_context_params->channel_id_path = channel_id_path;

    return network_context_params;
  }

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  base::FilePath user_data_dir_;
  NetworkContextParamsFactory network_context_params_factory_;
  network::mojom::NetworkContextPtr network_context_;
  network::mojom::URLLoaderFactoryPtr url_loader_factory_;
  scoped_refptr<InternalState> internal_state_;

  DISALLOW_COPY_AND_ASSIGN(SharedURLLoaderFactory);
};

SafeBrowsingNetworkContext::SafeBrowsingNetworkContext(
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    const base::FilePath& user_data_dir,
    NetworkContextParamsFactory network_context_params_factory) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  url_loader_factory_ = base::MakeRefCounted<SharedURLLoaderFactory>(
      request_context_getter, user_data_dir,
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
