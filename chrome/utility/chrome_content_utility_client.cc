// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/chrome_content_utility_client.h"

#include <stddef.h>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "chrome/common/buildflags.h"
#include "components/mirroring/mojom/constants.mojom.h"
#include "components/mirroring/service/features.h"
#include "components/mirroring/service/mirroring_service.h"
#include "components/services/heap_profiling/heap_profiling_service.h"
#include "components/services/heap_profiling/public/mojom/constants.mojom.h"
#include "components/services/unzip/public/interfaces/constants.mojom.h"
#include "components/services/unzip/unzip_service.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/utility/utility_thread.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/cpp/features.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/embedded_service_info.h"
#include "services/service_manager/sandbox/switches.h"
#include "ui/base/ui_features.h"

#if !defined(OS_ANDROID)
#include "chrome/utility/importer/profile_import_impl.h"
#include "chrome/utility/importer/profile_import_service.h"
#include "components/services/patch/patch_service.h"  // nogncheck
#include "components/services/patch/public/interfaces/constants.mojom.h"  // nogncheck
#include "services/network/url_request_context_builder_mojo.h"
#include "services/proxy_resolver/proxy_resolver_service.h"  // nogncheck
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"  // nogncheck
#endif  // !defined(OS_ANDROID)

#if defined(OS_WIN)
#include "chrome/services/util_win/public/mojom/constants.mojom.h"
#include "chrome/services/util_win/util_win_service.h"
#endif

#if BUILDFLAG(ENABLE_ISOLATED_XR_SERVICE)
#include "chrome/services/isolated_xr_device/xr_device_service.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/services/removable_storage_writer/public/mojom/constants.mojom.h"
#include "chrome/services/removable_storage_writer/removable_storage_writer_service.h"
#if defined(OS_WIN)
#include "chrome/services/wifi_util_win/public/mojom/constants.mojom.h"
#include "chrome/services/wifi_util_win/wifi_util_win_service.h"
#endif
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_ANDROID)
#include "chrome/services/media_gallery_util/media_gallery_util_service.h"
#include "chrome/services/media_gallery_util/public/mojom/constants.mojom.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/utility/mash_service_factory.h"
#include "chromeos/services/ime/ime_service.h"
#include "chromeos/services/ime/public/mojom/constants.mojom.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/common/chrome_content_client.h"
#include "components/services/pdf_compositor/public/cpp/pdf_compositor_service_factory.h"  // nogncheck
#include "components/services/pdf_compositor/public/interfaces/pdf_compositor.mojom.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_PRINTING) && defined(OS_WIN))
#include "chrome/services/printing/printing_service.h"
#include "chrome/services/printing/public/mojom/constants.mojom.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING) && defined(OS_WIN)
#include "chrome/services/printing/pdf_to_emf_converter_factory.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && defined(OS_WIN)
#include "chrome/utility/printing_handler.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING) && defined(OS_CHROMEOS)
#include "chrome/services/cups_ipp_parser/cups_ipp_parser_service.h"  // nogncheck
#include "chrome/services/cups_ipp_parser/public/mojom/constants.mojom.h"  // nogncheck
#endif

#if defined(FULL_SAFE_BROWSING) || defined(OS_CHROMEOS)
#include "chrome/services/file_util/file_util_service.h"  // nogncheck
#include "chrome/services/file_util/public/mojom/constants.mojom.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_SIMPLE_BROWSER_SERVICE_OUT_OF_PROCESS)
#include "services/content/simple_browser/public/mojom/constants.mojom.h"  // nogncheck
#include "services/content/simple_browser/simple_browser_service.h"  // nogncheck
#endif

namespace {

base::LazyInstance<ChromeContentUtilityClient::NetworkBinderCreationCallback>::
    Leaky g_network_binder_creation_callback = LAZY_INSTANCE_INITIALIZER;

#if BUILDFLAG(ENABLE_EXTENSIONS)
void RegisterRemovableStorageWriterService(
    ChromeContentUtilityClient::StaticServiceMap* services) {
  service_manager::EmbeddedServiceInfo service_info;
  service_info.factory =
      base::BindRepeating(&RemovableStorageWriterService::CreateService);
  services->emplace(chrome::mojom::kRemovableStorageWriterServiceName,
                    service_info);
}
#endif

}  // namespace

ChromeContentUtilityClient::ChromeContentUtilityClient()
    : utility_process_running_elevated_(false) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && defined(OS_WIN)
  printing_handler_ = std::make_unique<printing::PrintingHandler>();
#endif

#if defined(OS_CHROMEOS)
  mash_service_factory_ = std::make_unique<MashServiceFactory>();
#endif
}

ChromeContentUtilityClient::~ChromeContentUtilityClient() = default;

void ChromeContentUtilityClient::UtilityThreadStarted() {
#if defined(OS_WIN)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  utility_process_running_elevated_ = command_line->HasSwitch(
      service_manager::switches::kNoSandboxAndElevatedPrivileges);
#endif

  content::ServiceManagerConnection* connection =
      content::ChildThread::Get()->GetServiceManagerConnection();

  // NOTE: Some utility process instances are not connected to the Service
  // Manager. Nothing left to do in that case.
  if (!connection)
    return;

  auto registry = std::make_unique<service_manager::BinderRegistry>();
  // If our process runs with elevated privileges, only add elevated Mojo
  // interfaces to the interface registry.
  if (!utility_process_running_elevated_) {
#if BUILDFLAG(ENABLE_PRINTING) && defined(OS_WIN)
    // TODO(crbug.com/798782): remove when the Cloud print chrome/service is
    // removed.
    registry->AddInterface(
        base::BindRepeating(printing::PdfToEmfConverterFactory::Create),
        base::ThreadTaskRunnerHandle::Get());
#endif
  }

  connection->AddConnectionFilter(
      std::make_unique<content::SimpleConnectionFilter>(std::move(registry)));
}

bool ChromeContentUtilityClient::OnMessageReceived(
    const IPC::Message& message) {
  if (utility_process_running_elevated_)
    return false;

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && defined(OS_WIN)
  if (printing_handler_->OnMessageReceived(message))
    return true;
#endif
  return false;
}

void ChromeContentUtilityClient::RegisterServices(
    ChromeContentUtilityClient::StaticServiceMap* services) {
  if (utility_process_running_elevated_) {
#if defined(OS_WIN) && BUILDFLAG(ENABLE_EXTENSIONS)
    service_manager::EmbeddedServiceInfo service_info;
    service_info.factory =
        base::BindRepeating(&WifiUtilWinService::CreateService);
    services->emplace(chrome::mojom::kWifiUtilWinServiceName, service_info);

    RegisterRemovableStorageWriterService(services);
#endif
    return;
  }

#if BUILDFLAG(ENABLE_ISOLATED_XR_SERVICE)
  service_manager::EmbeddedServiceInfo service_info;
  service_info.factory =
      base::BindRepeating(&device::XrDeviceService::CreateXrDeviceService);
  services->emplace(device::mojom::kVrIsolatedServiceName, service_info);
#endif

#if BUILDFLAG(ENABLE_PRINTING)
  service_manager::EmbeddedServiceInfo pdf_compositor_info;
  pdf_compositor_info.factory = base::BindRepeating(
      &printing::CreatePdfCompositorService, GetUserAgent());
  services->emplace(printing::mojom::kServiceName, pdf_compositor_info);
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_PRINTING) && defined(OS_WIN))
  service_manager::EmbeddedServiceInfo printing_info;
  printing_info.factory =
      base::BindRepeating(&printing::PrintingService::CreateService);
  services->emplace(printing::mojom::kChromePrintingServiceName, printing_info);
#endif

  service_manager::EmbeddedServiceInfo profiling_info;
  profiling_info.task_runner = content::ChildThread::Get()->GetIOTaskRunner();
  profiling_info.factory =
      base::BindRepeating(&heap_profiling::HeapProfilingService::CreateService);
  services->emplace(heap_profiling::mojom::kServiceName, profiling_info);

#if !defined(OS_ANDROID)
  service_manager::EmbeddedServiceInfo proxy_resolver_info;
  proxy_resolver_info.task_runner =
      content::ChildThread::Get()->GetIOTaskRunner();
  proxy_resolver_info.factory =
      base::BindRepeating(&proxy_resolver::ProxyResolverService::CreateService);
  services->emplace(proxy_resolver::mojom::kProxyResolverServiceName,
                    proxy_resolver_info);

  service_manager::EmbeddedServiceInfo profile_import_info;
  profile_import_info.factory =
      base::BindRepeating(&ProfileImportService::CreateService);
  services->emplace(chrome::mojom::kProfileImportServiceName,
                    profile_import_info);
#endif

#if defined(OS_WIN)
  {
    service_manager::EmbeddedServiceInfo service_info;
    service_info.factory = base::BindRepeating(&UtilWinService::CreateService);
    services->emplace(chrome::mojom::kUtilWinServiceName, service_info);
  }
#endif

#if BUILDFLAG(ENABLE_PRINTING) && defined(OS_CHROMEOS)
  {
    service_manager::EmbeddedServiceInfo service_info;
    service_info.factory =
        base::BindRepeating(&CupsIppParserService::CreateService);
    services->emplace(chrome::mojom::kCupsIppParserServiceName, service_info);
  }
#endif

#if defined(FULL_SAFE_BROWSING) || defined(OS_CHROMEOS)
  {
    service_manager::EmbeddedServiceInfo service_info;
    service_info.factory = base::BindRepeating(&FileUtilService::CreateService);
    services->emplace(chrome::mojom::kFileUtilServiceName, service_info);
  }
#endif

#if !defined(OS_ANDROID)
  {
    service_manager::EmbeddedServiceInfo service_info;
    service_info.factory =
        base::BindRepeating(&patch::PatchService::CreateService);
    services->emplace(patch::mojom::kServiceName, service_info);
  }
#endif

  {
    service_manager::EmbeddedServiceInfo service_info;
    service_info.factory =
        base::BindRepeating(&unzip::UnzipService::CreateService);
    services->emplace(unzip::mojom::kServiceName, service_info);
  }

#if BUILDFLAG(ENABLE_EXTENSIONS) && !defined(OS_WIN)
  // On Windows the service is running elevated.
  RegisterRemovableStorageWriterService(services);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && !defined(OS_WIN)

#if BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_ANDROID)
  {
    service_manager::EmbeddedServiceInfo service_info;
    service_info.factory =
        base::BindRepeating(&MediaGalleryUtilService::CreateService);
    services->emplace(chrome::mojom::kMediaGalleryUtilServiceName,
                      service_info);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_ANDROID)

#if !defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(mirroring::features::kMirroringService) &&
      base::FeatureList::IsEnabled(features::kAudioServiceAudioStreams) &&
      base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    service_manager::EmbeddedServiceInfo mirroring_info;
    mirroring_info.factory = base::BindRepeating(
        [](scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
            -> std::unique_ptr<service_manager::Service> {
          return std::make_unique<mirroring::MirroringService>(
              std::move(io_task_runner));
        },
        content::ChildThread::Get()->GetIOTaskRunner());
    services->emplace(mirroring::mojom::kServiceName, mirroring_info);
  }
#endif

#if defined(OS_CHROMEOS)
  // TODO(jamescook): Figure out why we have to do this when not using mash.
  mash_service_factory_->RegisterOutOfProcessServices(services);

  {
    service_manager::EmbeddedServiceInfo service_info;
    service_info.factory =
        base::BindRepeating(&chromeos::ime::CreateImeService);
    services->emplace(chromeos::ime::mojom::kServiceName, service_info);
  }
#endif

#if BUILDFLAG(ENABLE_SIMPLE_BROWSER_SERVICE_OUT_OF_PROCESS)
  {
    service_manager::EmbeddedServiceInfo service_info;
    service_info.factory =
        base::BindRepeating([]() -> std::unique_ptr<service_manager::Service> {
          return std::make_unique<simple_browser::SimpleBrowserService>(
              simple_browser::SimpleBrowserService::UIInitializationMode::
                  kInitializeUI);
        });
    services->emplace(simple_browser::mojom::kServiceName, service_info);
  }
#endif
}

void ChromeContentUtilityClient::RegisterNetworkBinders(
    service_manager::BinderRegistry* registry) {
  if (g_network_binder_creation_callback.Get())
    g_network_binder_creation_callback.Get().Run(registry);
}

// static
void ChromeContentUtilityClient::SetNetworkBinderCreationCallback(
    const NetworkBinderCreationCallback& callback) {
  g_network_binder_creation_callback.Get() = callback;
}
