// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/url_loader_factory_provider.h"

#include <memory>
#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/enterprise_companion/enterprise_companion.h"
#include "chrome/enterprise_companion/event_logger.h"
#include "chrome/enterprise_companion/proxy_config_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/cert/cert_verifier.h"
#include "net/cert_net/cert_net_fetcher_url_request.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/transitional_url_loader_factory_owner.h"

#if BUILDFLAG(IS_MAC)
#include <sys/types.h>

#include "chrome/enterprise_companion/mac/mac_utils.h"
#endif

namespace enterprise_companion {

namespace {

std::unique_ptr<net::ProxyConfigService> CreateDefaultProxyConfigService(
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner) {
  std::unique_ptr<net::ProxyConfigService> system_config_service =
      net::ProxyConfigService::CreateSystemProxyConfigService(
          network_task_runner);
  // The Chromium updater only respects policy-provided proxy configurations on
  // Windows. For parity, the enterprise companion should do the same. Once the
  // product has launched, it would be worth experimenting with this
  // functionality on Mac. The updater is not productized on Linux so parity is
  // not a requirement.
#if BUILDFLAG(IS_MAC)
  return system_config_service;
#else
  return CreatePolicyProxyConfigService(
      device_management_storage::GetDefaultDMStorage(),
      GetDefaultSystemPolicyProxyConfigProvider(),
      std::move(system_config_service));
#endif
}

class URLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  explicit URLRequestContextGetter(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
      : network_task_runner_(network_task_runner),
        proxy_config_service_(
            CreateDefaultProxyConfigService(network_task_runner_)) {}

  URLRequestContextGetter(const URLRequestContextGetter&) = delete;
  URLRequestContextGetter& operator=(const URLRequestContextGetter&) = delete;

  // Overrides for net::URLRequestContextGetter.
  net::URLRequestContext* GetURLRequestContext() override {
    if (!url_request_context_.get()) {
      net::URLRequestContextBuilder builder;
      builder.DisableHttpCache();
      builder.set_proxy_config_service(std::move(proxy_config_service_));
      cert_net_fetcher_ = base::MakeRefCounted<net::CertNetFetcherURLRequest>();
      auto cert_verifier = net::CertVerifier::CreateDefault(cert_net_fetcher_);
      builder.SetCertVerifier(std::move(cert_verifier));
      url_request_context_ = builder.Build();
      cert_net_fetcher_->SetURLRequestContext(url_request_context_.get());
    }
    return url_request_context_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return network_task_runner_;
  }

 protected:
  ~URLRequestContextGetter() override { cert_net_fetcher_->Shutdown(); }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  scoped_refptr<net::CertNetFetcherURLRequest> cert_net_fetcher_;
};

// A URLLoaderFactory which forwards all calls to a remote. This exists as a
// hack to allow a disconnect callback to be configured on a receiver which gets
// bound via `network::SharedURLLoaderFactory::Clone`.
class URLLoaderFactoryProxy final : public network::mojom::URLLoaderFactory {
 public:
  explicit URLLoaderFactoryProxy(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote)
      : remote_(std::move(pending_remote)) {}

  ~URLLoaderFactoryProxy() override = default;

  // Overrides for network::mojom::URLLoaderFactory.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) final {
    return remote_->CreateLoaderAndStart(std::move(loader), request_id, options,
                                         request, std::move(client),
                                         traffic_annotation);
  }

  void Clone(mojo::PendingReceiver<URLLoaderFactory> receiver) final {
    return remote_->Clone(std::move(receiver));
  }

 private:
  mojo::Remote<network::mojom::URLLoaderFactory> remote_;
};

// Services network requests in this process.
class InProcessURLLoaderFactoryProvider : public URLLoaderFactoryProvider {
 public:
  InProcessURLLoaderFactoryProvider(
      base::SequenceBound<EventLoggerCookieHandler> event_logger_cookie_handler,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
      base::OnceClosure on_disconnect_callback)
      : url_loader_factory_owner_(
            base::MakeRefCounted<URLRequestContextGetter>(
                base::SingleThreadTaskRunner::GetCurrentDefault()),
            /*is_trusted=*/true),
        event_logger_cookie_handler_(std::move(event_logger_cookie_handler)) {
    if (pending_receiver.is_valid()) {
      // Bind the incoming receiver to the URL loader factory indirectly
      // through a self-owned `URLLoaderFactoryProxy` receiver, allowing a
      // disconnect callback to be configured. Without this indirection, a
      // disconnect handler can not be set without refactoring //network code.
      mojo::PendingRemote<network::mojom::URLLoaderFactory> remote;
      url_loader_factory_owner_.GetURLLoaderFactory()->Clone(
          remote.InitWithNewPipeAndPassReceiver());
      mojo::MakeSelfOwnedReceiver(
          std::make_unique<URLLoaderFactoryProxy>(std::move(remote)),
          std::move(pending_receiver))
          ->set_connection_error_handler(
              base::BindOnce(std::move(on_disconnect_callback)));
    }

    if (event_logger_cookie_handler_) {
      mojo::PendingRemote<network::mojom::CookieManager>
          cookie_manager_pending_remote;
      url_loader_factory_owner_.GetNetworkContext()->GetCookieManager(
          cookie_manager_pending_remote.InitWithNewPipeAndPassReceiver());
      event_logger_cookie_handler_.AsyncCall(&EventLoggerCookieHandler::Init)
          .WithArgs(std::move(cookie_manager_pending_remote),
                    base::DoNothing());
    }
  }

  ~InProcessURLLoaderFactoryProvider() override = default;

  std::unique_ptr<network::PendingSharedURLLoaderFactory>
  GetPendingURLLoaderFactory() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return url_loader_factory_owner_.GetURLLoaderFactory()->Clone();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  network::TransitionalURLLoaderFactoryOwner url_loader_factory_owner_;
  base::SequenceBound<EventLoggerCookieHandler> event_logger_cookie_handler_;
};

#if BUILDFLAG(IS_MAC)
// Delegates network requests to a remote process.
class URLLoaderFactoryProviderProxy : public URLLoaderFactoryProvider {
 public:
  explicit URLLoaderFactoryProviderProxy(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote,
      base::OnceClosure on_disconnect_callback) {
    mojo::Remote remote(std::move(pending_remote));
    remote.set_disconnect_handler(std::move(on_disconnect_callback));
    url_loader_factory_ =
        base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
            std::move(remote));
  }

  ~URLLoaderFactoryProviderProxy() override = default;

  std::unique_ptr<network::PendingSharedURLLoaderFactory>
  GetPendingURLLoaderFactory() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return url_loader_factory_->Clone();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};
#endif  // BUILDFLAG(IS_MAC)

}  // namespace

base::SequenceBound<URLLoaderFactoryProvider>
CreateInProcessUrlLoaderFactoryProvider(
    scoped_refptr<base::SingleThreadTaskRunner> net_thread_runner,
    base::SequenceBound<EventLoggerCookieHandler> event_logger_cookie_handler,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
    base::OnceClosure on_disconnect_callback) {
  return base::SequenceBound<InProcessURLLoaderFactoryProvider>(
      net_thread_runner, std::move(event_logger_cookie_handler),
      std::move(pending_receiver), std::move(on_disconnect_callback));
}

#if BUILDFLAG(IS_MAC)
base::SequenceBound<URLLoaderFactoryProvider>
CreateUrlLoaderFactoryProviderProxy(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote,
    base::OnceClosure on_disconnect_callback) {
  return base::SequenceBound<URLLoaderFactoryProviderProxy>(
      task_runner, std::move(pending_remote),
      std::move(on_disconnect_callback));
}

base::SequenceBound<URLLoaderFactoryProvider> CreateOutOfProcessNetWorker(
    base::OnceClosure on_disconnect_callback) {
  mojo::PlatformChannel channel;
  base::LaunchOptions options;
  base::FilePath exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &exe_path)) {
    LOG(ERROR) << "Failed to retrieve the current executable's path.";
    return {};
  }

  base::CommandLine command_line(exe_path);
  command_line.AppendSwitch(kNetWorkerSwitch);

  // Attempt to execute the network process in the bootstrap context of the
  // logged in user to pick up their proxy configuration and authorization. For
  // background, see the Apple Documentation Archive's entry on "Bootstrap
  // Contexts".
  std::optional<uid_t> uid = GuessLoggedInUser();
  if (!uid) {
    LOG(ERROR)
        << "Could not determine a logged-in user to impersonate for "
           "networking. The root bootstrap namespace (in formal Mach kernel "
           "terms, the \"startup context\") will be used, which may cause "
           "proxy configuration or authorization to fail.";
  } else {
    command_line.PrependWrapper(
        base::StringPrintf("/bin/launchctl asuser %d", *uid));
  }

  channel.PrepareToPassRemoteEndpoint(&options, &command_line);
  base::Process process = base::LaunchProcess(command_line, options);
  channel.RemoteProcessLaunchAttempted();
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to launch network process.";
    return {};
  }

  mojo::ScopedMessagePipeHandle pipe = mojo::OutgoingInvitation::SendIsolated(
      channel.TakeLocalEndpoint(), {}, process.Handle());
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote(
      std::move(pipe), network::mojom::URLLoaderFactory::Version_);
  if (!pending_remote) {
    LOG(ERROR) << "Failed to establish IPC with the network process.";
    return {};
  }

  return CreateUrlLoaderFactoryProviderProxy(
      base::SequencedTaskRunner::GetCurrentDefault(), std::move(pending_remote),
      std::move(on_disconnect_callback));
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace enterprise_companion
