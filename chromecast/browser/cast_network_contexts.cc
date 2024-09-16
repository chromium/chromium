// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_network_contexts.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_http_user_agent_settings.h"
#include "chromecast/common/user_agent.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace chromecast {
namespace shell {

namespace {

constexpr char kCookieStoreFile[] = "Cookies";

ContentSettingPatternSource CreateContentSetting(
    const std::string& primary_pattern,
    const std::string& secondary_pattern,
    ContentSetting setting) {
  return ContentSettingPatternSource(
      ContentSettingsPattern::FromString(primary_pattern),
      ContentSettingsPattern::FromString(secondary_pattern),
      base::Value(setting), content_settings::ProviderType::kNone,
      /*incognito=*/false);
}

}  // namespace

// SharedURLLoaderFactory backed by a CastNetworkContexts and its system
// NetworkContext. Transparently handles crashes.
class CastNetworkContexts::URLLoaderFactoryForSystem
    : public network::SharedURLLoaderFactory {
 public:
  explicit URLLoaderFactoryForSystem(CastNetworkContexts* network_context)
      : network_context_(network_context) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  URLLoaderFactoryForSystem(const URLLoaderFactoryForSystem&) = delete;
  URLLoaderFactoryForSystem& operator=(const URLLoaderFactoryForSystem&) =
      delete;

  // mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!network_context_)
      return;
    network_context_->GetSystemURLLoaderFactory()->CreateLoaderAndStart(
        std::move(receiver), request_id, options, url_request,
        std::move(client), traffic_annotation);
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    if (!network_context_)
      return;
    network_context_->GetSystemURLLoaderFactory()->Clone(std::move(receiver));
  }

  // SharedURLLoaderFactory implementation:
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
        this);
  }

  void Shutdown() { network_context_ = nullptr; }

 private:
  friend class base::RefCounted<URLLoaderFactoryForSystem>;
  ~URLLoaderFactoryForSystem() override {}

  SEQUENCE_CHECKER(sequence_checker_);
  CastNetworkContexts* network_context_;
};

CastNetworkContexts::CastNetworkContexts(
    std::vector<std::string> cors_exempt_headers_list)
    : cors_exempt_headers_list_(std::move(cors_exempt_headers_list)),
      system_shared_url_loader_factory_(
          base::MakeRefCounted<URLLoaderFactoryForSystem>(this)) {}

CastNetworkContexts::~CastNetworkContexts() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  system_shared_url_loader_factory_->Shutdown();
}

network::mojom::NetworkContext* CastNetworkContexts::GetSystemContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!system_network_context_ || !system_network_context_.is_connected()) {
    // This should call into OnNetworkServiceCreated(), which will re-create
    // the network service, if needed. There's a chance that it won't be
    // invoked, if the NetworkContext has encountered an error but the
    // NetworkService has not yet noticed its pipe was closed. In that case,
    // trying to create a new NetworkContext would fail, anyways, and hopefully
    // a new NetworkContext will be created on the next GetContext() call.
    content::GetNetworkService();
    DCHECK(system_network_context_);
  }

  return system_network_context_.get();
}

network::mojom::URLLoaderFactory*
CastNetworkContexts::GetSystemURLLoaderFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Create the URLLoaderFactory as needed.
  if (system_url_loader_factory_ && system_url_loader_factory_.is_connected()) {
    return system_url_loader_factory_.get();
  }

  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = network::mojom::kBrowserProcessId;
  params->is_orb_enabled = false;
  params->is_trusted = true;
  GetSystemContext()->CreateURLLoaderFactory(
      system_url_loader_factory_.BindNewPipeAndPassReceiver(),
      std::move(params));
  return system_shared_url_loader_factory_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
CastNetworkContexts::GetSystemSharedURLLoaderFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return system_shared_url_loader_factory_;
}

void CastNetworkContexts::SetAllowedDomainsForPersistentCookies(
    std::vector<std::string> allowed_domains_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  allowed_domains_for_persistent_cookies_ = std::move(allowed_domains_list);
}

void CastNetworkContexts::ConfigureNetworkContextParams(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ConfigureDefaultNetworkContextParams(network_context_params);

  // Copy of what's in ContentBrowserClient::CreateNetworkContext for now.
  network_context_params->accept_language = "en-us,en";
}

void CastNetworkContexts::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  // Disable QUIC if instructed by DCS. This remains constant for the lifetime
  // of the process.
  if (!chromecast::IsFeatureEnabled(kEnableQuic))
    network_service->DisableQuic();

  network_service->CreateNetworkContext(
      system_network_context_.BindNewPipeAndPassReceiver(),
      CreateSystemNetworkContextParams());
}

void CastNetworkContexts::OnLocaleUpdate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto accept_language = CastHttpUserAgentSettings::AcceptLanguage();

  GetSystemContext()->SetAcceptLanguage(accept_language);

  auto* browser_context = CastBrowserProcess::GetInstance()->browser_context();
  browser_context->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->SetAcceptLanguage(accept_language);
}

void CastNetworkContexts::OnPrefServiceShutdown() {
  if (proxy_config_service_)
    proxy_config_service_->RemoveObserver(this);

  if (pref_proxy_config_tracker_impl_)
    pref_proxy_config_tracker_impl_->DetachFromPrefService();
}

void CastNetworkContexts::ConfigureDefaultNetworkContextParams(
    network::mojom::NetworkContextParams* network_context_params) {
  network_context_params->http_cache_enabled = false;
  network_context_params->user_agent = GetUserAgent();
  network_context_params->accept_language =
      CastHttpUserAgentSettings::AcceptLanguage();

  auto* browser_context = CastBrowserProcess::GetInstance()->browser_context();
  DCHECK(browser_context);
  network_context_params->file_paths =
      network::mojom::NetworkContextFilePaths::New();
  network_context_params->file_paths->data_directory =
      browser_context->GetPath();
  network_context_params->file_paths->cookie_database_name =
      base::FilePath(kCookieStoreFile);
  network_context_params->restore_old_session_cookies = false;
  network_context_params->persist_session_cookies = true;
  network_context_params->cookie_manager_params = CreateCookieManagerParams();

  // Disable idle sockets close on memory pressure, if instructed by DCS. On
  // memory constrained devices:
  // 1. if idle sockets are closed when memory pressure happens, cast_shell will
  // close and re-open lots of connections to server.
  // 2. if idle sockets are kept alive when memory pressure happens, this may
  // cause JS engine gc frequently, leading to JS suspending.
  network_context_params->disable_idle_sockets_close_on_memory_pressure =
      IsFeatureEnabled(kDisableIdleSocketsCloseOnMemoryPressure);

  AddProxyToNetworkContextParams(network_context_params);

  network_context_params->cors_exempt_header_list.insert(
      network_context_params->cors_exempt_header_list.end(),
      cors_exempt_headers_list_.begin(), cors_exempt_headers_list_.end());
}

network::mojom::NetworkContextParamsPtr
CastNetworkContexts::CreateSystemNetworkContextParams() {
  network::mojom::NetworkContextParamsPtr network_context_params =
      network::mojom::NetworkContextParams::New();
  ConfigureDefaultNetworkContextParams(network_context_params.get());

  network_context_params->cert_verifier_params = content::GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());

  return network_context_params;
}

network::mojom::CookieManagerParamsPtr
CastNetworkContexts::CreateCookieManagerParams() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto params = network::mojom::CookieManagerParams::New();
  if (allowed_domains_for_persistent_cookies_.empty()) {
    // Don't restrict persistent cookie access if no allowlist is set.
    return params;
  }

  ContentSettingsForOneType settings;
  ContentSettingsForOneType settings_for_storage_access;
  ContentSettingsForOneType settings_for_top_level_storage_access;

  // Grant cookie and storage access to domains in the allowlist.
  for (const auto& domain : allowed_domains_for_persistent_cookies_) {
    auto allow_storage_access_setting = CreateContentSetting(
        /*primary_pattern=*/base::StrCat({"[*.]", domain}),
        /*secondary_pattern=*/"*", ContentSetting::CONTENT_SETTING_ALLOW);
    settings.push_back(allow_storage_access_setting);
    settings_for_storage_access.push_back(
        std::move(allow_storage_access_setting));

    // TODO(crbug.com/40246640): Consolidate this with the regular
    // STORAGE_ACCESS setting as usage becomes better-defined.
    auto allow_top_level_storage_access_setting = CreateContentSetting(
        /*primary_pattern=*/base::StrCat({"[*.]", domain}),
        /*secondary_pattern=*/"*", ContentSetting::CONTENT_SETTING_ALLOW);
    settings_for_top_level_storage_access.push_back(
        std::move(allow_top_level_storage_access_setting));
  }

  // Restrict cookie access to session only and block storage access for
  // domains not in the allowlist.
  // Note: storage access control depends on the feature |kStorageAccessAPI|
  // which has not been enabled by default in chromium.
  settings.push_back(CreateContentSetting(
      /*primary_pattern=*/"*",
      /*secondary_pattern=*/"*", ContentSetting::CONTENT_SETTING_SESSION_ONLY));
  settings_for_storage_access.push_back(CreateContentSetting(
      /*primary_pattern=*/"*",
      /*secondary_pattern=*/"*", ContentSetting::CONTENT_SETTING_BLOCK));
  settings_for_top_level_storage_access.push_back(CreateContentSetting(
      /*primary_pattern=*/"*",
      /*secondary_pattern=*/"*", ContentSetting::CONTENT_SETTING_BLOCK));
  params->content_settings[ContentSettingsType::COOKIES] = std::move(settings);
  params->content_settings[ContentSettingsType::STORAGE_ACCESS] =
      std::move(settings_for_storage_access);
  params->content_settings[ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS] =
      std::move(settings_for_top_level_storage_access);

  return params;
}

void CastNetworkContexts::AddProxyToNetworkContextParams(
    network::mojom::NetworkContextParams* network_context_params) {
  if (!proxy_config_service_) {
    pref_proxy_config_tracker_impl_ =
        std::make_unique<PrefProxyConfigTrackerImpl>(
            CastBrowserProcess::GetInstance()->pref_service(), nullptr);
    proxy_config_service_ =
        pref_proxy_config_tracker_impl_->CreateTrackingProxyConfigService(
            nullptr);
    proxy_config_service_->AddObserver(this);
  }

  mojo::PendingRemote<network::mojom::ProxyConfigClient> proxy_config_client;
  network_context_params->proxy_config_client_receiver =
      proxy_config_client.InitWithNewPipeAndPassReceiver();
  proxy_config_client_set_.Add(std::move(proxy_config_client));

  poller_receiver_set_.Add(this,
                           network_context_params->proxy_config_poller_client
                               .InitWithNewPipeAndPassReceiver());

  net::ProxyConfigWithAnnotation proxy_config;
  net::ProxyConfigService::ConfigAvailability availability =
      proxy_config_service_->GetLatestProxyConfig(&proxy_config);
  if (availability != net::ProxyConfigService::CONFIG_PENDING)
    network_context_params->initial_proxy_config = proxy_config;
}

void CastNetworkContexts::OnProxyConfigChanged(
    const net::ProxyConfigWithAnnotation& config,
    net::ProxyConfigService::ConfigAvailability availability) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (const auto& proxy_config_client : proxy_config_client_set_) {
    switch (availability) {
      case net::ProxyConfigService::CONFIG_VALID:
        proxy_config_client->OnProxyConfigUpdated(config);
        break;
      case net::ProxyConfigService::CONFIG_UNSET:
        proxy_config_client->OnProxyConfigUpdated(
            net::ProxyConfigWithAnnotation::CreateDirect());
        break;
      case net::ProxyConfigService::CONFIG_PENDING:
        NOTREACHED();
    }
  }
}

void CastNetworkContexts::OnLazyProxyConfigPoll() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  proxy_config_service_->OnLazyPoll();
}

}  // namespace shell
}  // namespace chromecast
