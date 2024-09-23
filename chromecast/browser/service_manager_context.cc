// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/service_manager_context.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/process/process_handle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chromecast/browser/cast_content_browser_client.h"
#include "chromecast/browser/service_manager_connection.h"
#include "chromecast/browser/system_connector.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/service_manager.h"
#include "services/service_manager/service_process_host.h"
#include "services/service_manager/service_process_launcher.h"
#include "ui/base/buildflags.h"
#include "ui/base/ui_base_features.h"

namespace chromecast {

namespace {

const char kSystemServiceName[] = "content_system";

base::LazyInstance<std::unique_ptr<service_manager::Connector>>::Leaky
    g_io_thread_connector = LAZY_INSTANCE_INITIALIZER;

const service_manager::Manifest& GetBrowserManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(ServiceManagerContext::kBrowserServiceName)
          .WithDisplayName("Browser process")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .CanConnectToInstancesInAnyGroup(true)
                           .CanConnectToInstancesWithAnyId(true)
                           .CanRegisterOtherServiceInstances(true)
                           .Build())
          .RequireCapability("*", "app")
          .RequireCapability("*", "multizone")
          .RequireCapability("*", "reconnect")
          .RequireCapability("*", "renderer")
          .Build()};
  return *manifest;
}

service_manager::Manifest GetSystemManifest(
    shell::CastContentBrowserClient* cast_content_browser_client) {
  // TODO(crbug.com/40626947): This is a bit of a temporary hack so that
  // we can make the global service instance a singleton. For now we just mirror
  // the per-BrowserContext manifest (formerly also used for the global
  // singleton instance), sans packaged services, since those are only meant to
  // be tied to a BrowserContext. The per-BrowserContext service should go away
  // soon, and then this can be removed.
  service_manager::Manifest manifest = GetBrowserManifest();
  manifest.Amend(cast_content_browser_client
                     ->GetServiceManifestOverlay(
                         ServiceManagerContext::kBrowserServiceName)
                     .value_or(service_manager::Manifest()));
  manifest.service_name = kSystemServiceName;
  manifest.packaged_services.clear();
  manifest.options.instance_sharing_policy =
      service_manager::Manifest::InstanceSharingPolicy::kSingleton;
  return manifest;
}

void DestroyConnectorOnIOThread() {
  g_io_thread_connector.Get().reset();
}

// A ServiceProcessHost implementation which uses the Service Manager's builtin
// service executable launcher. Not yet intended for use in production Chrome,
// hence availability is gated behind a flag.
class ServiceExecutableProcessHost
    : public service_manager::ServiceProcessHost {
 public:
  explicit ServiceExecutableProcessHost(const base::FilePath& executable_path)
      : launcher_(nullptr, executable_path) {}

  ServiceExecutableProcessHost(const ServiceExecutableProcessHost&) = delete;
  ServiceExecutableProcessHost& operator=(const ServiceExecutableProcessHost&) =
      delete;

  ~ServiceExecutableProcessHost() override = default;

  // service_manager::ServiceProcessHost:
  mojo::PendingRemote<service_manager::mojom::Service> Launch(
      const service_manager::Identity& identity,
      sandbox::mojom::Sandbox sandbox_type,
      const std::u16string& display_name,
      LaunchCallback callback) override {
    // TODO(crbug.com/41353434): Support sandboxing.
    return launcher_.Start(identity, sandbox::mojom::Sandbox::kNoSandbox,
                           std::move(callback));
  }

 private:
  service_manager::ServiceProcessLauncher launcher_;
};

using ServiceRequestHandler = base::RepeatingCallback<void(
    const service_manager::Identity& identity,
    mojo::PendingReceiver<service_manager::mojom::Service> receiver)>;

// Implements in- and out-of-process service instance launching for services
// built into the Content embedder's binary.
//
// All methods on this object (except the constructor) are called on the Service
// Manager's thread, which is effectively the browser's IO thread.
class BrowserServiceManagerDelegate
    : public service_manager::ServiceManager::Delegate {
 public:
  BrowserServiceManagerDelegate(
      const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner,
      ServiceRequestHandler main_thread_request_handler)
      : main_thread_task_runner_(main_thread_task_runner),
        main_thread_request_handler_(std::move(main_thread_request_handler)) {}

  BrowserServiceManagerDelegate(const BrowserServiceManagerDelegate&) = delete;
  BrowserServiceManagerDelegate& operator=(
      const BrowserServiceManagerDelegate&) = delete;

  ~BrowserServiceManagerDelegate() override = default;

  // service_manager::ServiceManager::Delegate:
  bool RunBuiltinServiceInstanceInCurrentProcess(
      const service_manager::Identity& identity,
      mojo::PendingReceiver<service_manager::mojom::Service> receiver)
      override {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(main_thread_request_handler_, identity,
                                  std::move(receiver)));
    return true;
  }

  std::unique_ptr<service_manager::ServiceProcessHost>
  CreateProcessHostForBuiltinServiceInstance(
      const service_manager::Identity& identity) override {
    // Cast only uses the default kInProcessBuiltin mode. This function should
    // only be called for kOutOfProcessBuiltin mode.
    NOTREACHED();
  }

  std::unique_ptr<service_manager::ServiceProcessHost>
  CreateProcessHostForServiceExecutable(
      const base::FilePath& executable_path) override {
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableServiceBinaryLauncher)) {
      return nullptr;
    }

    return std::make_unique<ServiceExecutableProcessHost>(executable_path);
  }

 private:
  const scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;
  const ServiceRequestHandler main_thread_request_handler_;
};

}  // namespace

const char ServiceManagerContext::kBrowserServiceName[] = "content_browser";

// State which lives on the IO thread and drives the ServiceManager.
class ServiceManagerContext::InProcessServiceManagerContext
    : public base::RefCountedThreadSafe<InProcessServiceManagerContext> {
 public:
  InProcessServiceManagerContext(scoped_refptr<base::SingleThreadTaskRunner>
                                     service_manager_thread_task_runner)
      : service_manager_thread_task_runner_(
            service_manager_thread_task_runner) {}

  InProcessServiceManagerContext(const InProcessServiceManagerContext&) =
      delete;
  InProcessServiceManagerContext& operator=(
      const InProcessServiceManagerContext&) = delete;

  void Start(std::vector<service_manager::Manifest> manifests,
             mojo::PendingRemote<service_manager::mojom::Service> system_remote,
             ServiceRequestHandler request_handler) {
    service_manager_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &InProcessServiceManagerContext::StartOnServiceManagerThread, this,
            std::move(manifests),
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            std::move(system_remote), std::move(request_handler)));
  }

  void ShutDown() {
    service_manager_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &InProcessServiceManagerContext::ShutDownOnServiceManagerThread,
            this));
  }

  void StartServices(std::vector<std::string> service_names) {
    service_manager_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&InProcessServiceManagerContext ::
                                      StartServicesOnServiceManagerThread,
                                  this, std::move(service_names)));
  }

 private:
  friend class base::RefCountedThreadSafe<InProcessServiceManagerContext>;

  ~InProcessServiceManagerContext() = default;

  void StartOnServiceManagerThread(
      std::vector<service_manager::Manifest> manifests,
      scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
      mojo::PendingRemote<service_manager::mojom::Service> system_remote,
      ServiceRequestHandler request_handler) {
    service_manager_ = std::make_unique<service_manager::ServiceManager>(
        std::move(manifests),
        std::make_unique<BrowserServiceManagerDelegate>(
            ui_thread_task_runner, std::move(request_handler)));

    mojo::Remote<service_manager::mojom::ProcessMetadata> metadata;
    service_manager_->RegisterService(
        service_manager::Identity(kSystemServiceName,
                                  service_manager::kSystemInstanceGroup,
                                  base::Token{}, base::Token::CreateRandom()),
        std::move(system_remote), metadata.BindNewPipeAndPassReceiver());
    metadata->SetPID(base::GetCurrentProcId());
  }

  void ShutDownOnServiceManagerThread() { service_manager_.reset(); }

  void StartServicesOnServiceManagerThread(
      std::vector<std::string> service_names) {
    if (!service_manager_)
      return;

    for (const auto& service_name : service_names)
      service_manager_->StartService(service_name);
  }

  const scoped_refptr<base::SingleThreadTaskRunner>
      service_manager_thread_task_runner_;
  std::unique_ptr<service_manager::ServiceManager> service_manager_;
};

ServiceManagerContext::ServiceManagerContext(
    shell::CastContentBrowserClient* cast_content_browser_client,
    scoped_refptr<base::SingleThreadTaskRunner>
        service_manager_thread_task_runner)
    : cast_content_browser_client_(cast_content_browser_client),
      service_manager_thread_task_runner_(
          std::move(service_manager_thread_task_runner)) {
  // The |service_manager_thread_task_runner_| must have been created before
  // starting the ServiceManager.
  DCHECK(service_manager_thread_task_runner_);
  std::vector<service_manager::Manifest> manifests;
  manifests.push_back(GetBrowserManifest());
  manifests.push_back(GetSystemManifest(cast_content_browser_client_));
  for (auto& manifest : manifests) {
    std::optional<service_manager::Manifest> overlay =
        cast_content_browser_client_->GetServiceManifestOverlay(
            manifest.service_name);
    if (overlay)
      manifest.Amend(*overlay);
  }
  for (auto& extra_manifest :
       cast_content_browser_client_->GetExtraServiceManifests()) {
    manifests.emplace_back(std::move(extra_manifest));
  }
  in_process_context_ =
      new InProcessServiceManagerContext(service_manager_thread_task_runner_);

  mojo::PendingRemote<service_manager::mojom::Service> system_remote;
  ServiceManagerConnection::SetForProcess(ServiceManagerConnection::Create(
      system_remote.InitWithNewPipeAndPassReceiver(),
      service_manager_thread_task_runner_));
  auto* system_connection = ServiceManagerConnection::GetForProcess();
  SetSystemConnector(system_connection->GetConnector()->Clone());

  // This is safe to assign directly from any thread, because
  // ServiceManagerContext must be constructed before anyone can call
  // GetConnectorForIOThread().
  g_io_thread_connector.Get() = system_connection->GetConnector()->Clone();

  in_process_context_->Start(
      manifests, std::move(system_remote),
      base::BindRepeating(&ServiceManagerContext::RunServiceInstance,
                          weak_ptr_factory_.GetWeakPtr()));
  in_process_context_->StartServices(
      cast_content_browser_client_->GetStartupServices());
}

ServiceManagerContext::~ServiceManagerContext() {
  ShutDown();
}

void ServiceManagerContext::ShutDown() {
  // NOTE: The in-process ServiceManager MUST be destroyed before the browser
  // process-wide ServiceManagerConnection. Otherwise it's possible for the
  // ServiceManager to receive connection requests for service:content_browser
  // which it may attempt to service by launching a new instance of the browser.
  if (in_process_context_)
    in_process_context_->ShutDown();
  if (ServiceManagerConnection::GetForProcess())
    ServiceManagerConnection::DestroyForProcess();
  service_manager_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DestroyConnectorOnIOThread));
}

// static
service_manager::Connector* ServiceManagerContext::GetConnectorForIOThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  return g_io_thread_connector.Get().get();
}

void ServiceManagerContext::RunServiceInstance(
    const service_manager::Identity& identity,
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  cast_content_browser_client_->RunServiceInstance(identity, &receiver);
  DLOG_IF(ERROR, receiver) << "Unhandled service request for \""
                           << identity.name() << "\"";
}

}  // namespace chromecast
