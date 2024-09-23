// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/services.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/services/speech/buildflags/buildflags.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/services/csv_password/csv_password_parser_impl.h"
#include "components/password_manager/services/csv_password/public/mojom/csv_password_parser.mojom.h"
#include "components/safe_browsing/buildflags.h"
#include "components/services/language_detection/language_detection_service_impl.h"
#include "components/services/language_detection/public/mojom/language_detection.mojom.h"
#include "components/services/on_device_translation/on_device_translation_service.h"
#include "components/services/patch/file_patcher_impl.h"
#include "components/services/patch/public/mojom/file_patcher.mojom.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "components/services/unzip/unzipper_impl.h"
#include "components/webapps/services/web_app_origin_association/public/mojom/web_app_origin_association_parser.mojom.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_parser_impl.h"
#include "content/public/utility/utility_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/service_factory.h"
#include "pdf/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/services/system_signals/win/win_system_signals_service.h"
#include "chrome/services/util_win/processor_metrics.h"
#include "chrome/services/util_win/public/mojom/util_read_icon.mojom.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "chrome/services/util_win/util_read_icon.h"
#include "chrome/services/util_win/util_win_impl.h"
#include "components/device_signals/core/common/mojom/system_signals.mojom.h"  // nogncheck
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"  // nogncheck
#include "components/services/quarantine/quarantine_impl.h"  // nogncheck
#include "services/proxy_resolver_win/public/mojom/proxy_resolver_win.mojom.h"
#include "services/proxy_resolver_win/windows_system_proxy_resolver_impl.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "chrome/services/mac_notifications/mac_notification_provider_impl.h"
#include "chrome/services/system_signals/mac/mac_system_signals_service.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX)
#include "chrome/services/system_signals/linux/linux_system_signals_service.h"
#endif  // BUILDFLAG(IS_LINUX)

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/common/importer/profile_import.mojom.h"
#include "chrome/utility/importer/profile_import_impl.h"
#include "components/mirroring/service/mirroring_service.h"
#include "services/passage_embeddings/passage_embeddings_service.h"
#include "services/proxy_resolver/proxy_resolver_factory_impl.h"  // nogncheck
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "services/screen_ai/public/mojom/screen_ai_factory.mojom.h"  // nogncheck
#include "services/screen_ai/screen_ai_service_impl.h"  // nogncheck
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_BROWSER_SPEECH_SERVICE)
#include "chrome/services/speech/speech_recognition_service_impl.h"  // nogncheck
#include "media/mojo/mojom/speech_recognition_service.mojom.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_BROWSER_SPEECH_SERVICE)

#if BUILDFLAG(FULL_SAFE_BROWSING) || BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/services/file_util/file_util_service.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/services/removable_storage_writer/public/mojom/removable_storage_writer.mojom.h"
#include "chrome/services/removable_storage_writer/removable_storage_writer.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) || BUILDFLAG(IS_ANDROID)
#include "chrome/services/media_gallery_util/media_parser_factory.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_PRINTING) && BUILDFLAG(IS_WIN))
#include "chrome/services/printing/printing_service.h"
#include "chrome/services/printing/public/mojom/printing_service.mojom.h"
#endif

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/services/printing/print_backend_service_impl.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/services/print_compositor/print_compositor_impl.h"  // nogncheck
#include "components/services/print_compositor/public/mojom/print_compositor.mojom.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_PRINTING)

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
#include "components/services/paint_preview_compositor/paint_preview_compositor_collection_impl.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
static_assert(BUILDFLAG(ENABLE_PDF), "ChromeOS Ash must enable PDF");
static_assert(BUILDFLAG(ENABLE_PRINTING), "ChromeOS Ash must enable Printing");
#include "chrome/services/ipp_parser/ipp_parser.h"  // nogncheck
#include "chrome/services/ipp_parser/public/mojom/ipp_parser.mojom.h"  // nogncheck
#include "chrome/services/pdf/pdf_service.h"
#include "chrome/services/pdf/public/mojom/pdf_service.mojom.h"
#include "chrome/services/sharing/sharing_impl.h"
#include "chromeos/ash/components/assistant/buildflags.h"  // nogncheck
#include "chromeos/ash/components/local_search_service/local_search_service.h"
#include "chromeos/ash/components/local_search_service/public/mojom/local_search_service.mojom.h"
#include "chromeos/ash/components/trash_service/public/mojom/trash_service.mojom.h"
#include "chromeos/ash/components/trash_service/trash_service_impl.h"
#include "chromeos/ash/services/ime/ime_service.h"
#include "chromeos/ash/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/sharing.mojom.h"  // nogncheck
#include "chromeos/ash/services/orca/orca_library.h"
#include "chromeos/ash/services/quick_pair/quick_pair_service.h"
#include "chromeos/ash/services/recording/recording_service.h"
#include "chromeos/constants/chromeos_features.h"  // nogncheck
#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "chromeos/services/tts/tts_service.h"

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "chromeos/ash/services/assistant/audio_decoder/assistant_audio_decoder_factory.h"  // nogncheck
#include "chromeos/ash/services/libassistant/libassistant_service.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/mahi/content_extraction_service.h"
#include "chromeos/components/mahi/public/mojom/content_extraction.mojom.h"
#include "chromeos/components/quick_answers/public/cpp/service/spell_check_service.h"
#include "chromeos/components/quick_answers/public/mojom/spell_check.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

auto RunFilePatcher(mojo::PendingReceiver<patch::mojom::FilePatcher> receiver) {
  return std::make_unique<patch::FilePatcherImpl>(std::move(receiver));
}

auto RunUnzipper(mojo::PendingReceiver<unzip::mojom::Unzipper> receiver) {
  return std::make_unique<unzip::UnzipperImpl>(std::move(receiver));
}

auto RunLanguageDetectionService(
    mojo::PendingReceiver<language_detection::mojom::LanguageDetectionService>
        receiver) {
  return std::make_unique<language_detection::LanguageDetectionServiceImpl>(
      std::move(receiver));
}

auto RunWebAppOriginAssociationParser(
    mojo::PendingReceiver<webapps::mojom::WebAppOriginAssociationParser>
        receiver) {
  return std::make_unique<webapps::WebAppOriginAssociationParserImpl>(
      std::move(receiver));
}

auto RunCSVPasswordParser(
    mojo::PendingReceiver<password_manager::mojom::CSVPasswordParser>
        receiver) {
  return std::make_unique<password_manager::CSVPasswordParserImpl>(
      std::move(receiver));
}

#if BUILDFLAG(IS_WIN)
auto RunProcessorMetrics(
    mojo::PendingReceiver<chrome::mojom::ProcessorMetrics> receiver) {
  return std::make_unique<ProcessorMetricsImpl>(std::move(receiver));
}

auto RunQuarantineService(
    mojo::PendingReceiver<quarantine::mojom::Quarantine> receiver) {
  return std::make_unique<quarantine::QuarantineImpl>(std::move(receiver));
}

auto RunWindowsUtility(mojo::PendingReceiver<chrome::mojom::UtilWin> receiver) {
  return std::make_unique<UtilWinImpl>(std::move(receiver));
}

auto RunWindowsIconReader(
    mojo::PendingReceiver<chrome::mojom::UtilReadIcon> receiver) {
  return std::make_unique<UtilReadIcon>(std::move(receiver));
}

auto RunWindowsSystemProxyResolver(
    mojo::PendingReceiver<proxy_resolver_win::mojom::WindowsSystemProxyResolver>
        receiver) {
  return std::make_unique<proxy_resolver_win::WindowsSystemProxyResolverImpl>(
      std::move(receiver));
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
auto RunMacNotificationService(
    mojo::PendingReceiver<mac_notifications::mojom::MacNotificationProvider>
        receiver) {
  return std::make_unique<mac_notifications::MacNotificationProviderImpl>(
      std::move(receiver));
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
auto RunSystemSignalsService(
    mojo::PendingReceiver<device_signals::mojom::SystemSignalsService>
        receiver) {
#if BUILDFLAG(IS_WIN)
  return std::make_unique<system_signals::WinSystemSignalsService>(
      std::move(receiver));
#elif BUILDFLAG(IS_MAC)
  return std::make_unique<system_signals::MacSystemSignalsService>(
      std::move(receiver));
#else
  return std::make_unique<system_signals::LinuxSystemSignalsService>(
      std::move(receiver));
#endif  // BUILDFLAG(IS_WIN)
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if !BUILDFLAG(IS_ANDROID)
auto RunProxyResolver(
    mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolverFactory>
        receiver) {
  return std::make_unique<proxy_resolver::ProxyResolverFactoryImpl>(
      std::move(receiver));
}

auto RunProfileImporter(
    mojo::PendingReceiver<chrome::mojom::ProfileImport> receiver) {
  return std::make_unique<ProfileImportImpl>(std::move(receiver));
}

auto RunMirroringService(
    mojo::PendingReceiver<mirroring::mojom::MirroringService> receiver) {
  return std::make_unique<mirroring::MirroringService>(
      std::move(receiver), content::UtilityThread::Get()->GetIOTaskRunner());
}

auto RunPassageEmbeddingsService(
    mojo::PendingReceiver<passage_embeddings::mojom::PassageEmbeddingsService>
        receiver) {
  return std::make_unique<passage_embeddings::PassageEmbeddingsService>(
      std::move(receiver));
}

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_BROWSER_SPEECH_SERVICE)
auto RunSpeechRecognitionService(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionService> receiver) {
  return std::make_unique<speech::SpeechRecognitionServiceImpl>(
      std::move(receiver));
}
#endif  // !BUILDFLAG(ENABLE_BROWSER_SPEECH_SERVICE)

#if !BUILDFLAG(IS_ANDROID)
auto RunScreenAIServiceFactory(
    mojo::PendingReceiver<screen_ai::mojom::ScreenAIServiceFactory> receiver) {
  return std::make_unique<screen_ai::ScreenAIService>(std::move(receiver));
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
auto RunCupsIppParser(
    mojo::PendingReceiver<ipp_parser::mojom::IppParser> receiver) {
  return std::make_unique<ipp_parser::IppParser>(std::move(receiver));
}
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING) || BUILDFLAG(IS_CHROMEOS_ASH)
auto RunFileUtil(
    mojo::PendingReceiver<chrome::mojom::FileUtilService> receiver) {
  return std::make_unique<FileUtilService>(std::move(receiver));
}
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
auto RunRemovableStorageWriter(
    mojo::PendingReceiver<chrome::mojom::RemovableStorageWriter> receiver) {
  return std::make_unique<RemovableStorageWriter>(std::move(receiver));
}
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) || BUILDFLAG(IS_ANDROID)
auto RunMediaParserFactory(
    mojo::PendingReceiver<chrome::mojom::MediaParserFactory> receiver) {
  return std::make_unique<MediaParserFactory>(std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
auto RunPdfService(mojo::PendingReceiver<pdf::mojom::PdfService> receiver) {
  return std::make_unique<pdf::PdfService>(std::move(receiver));
}
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_PRINTING) && BUILDFLAG(IS_WIN))
auto RunPrintingService(
    mojo::PendingReceiver<printing::mojom::PrintingService> receiver) {
  return std::make_unique<printing::PrintingService>(std::move(receiver));
}
#endif

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
auto RunPaintPreviewCompositor(
    mojo::PendingReceiver<
        paint_preview::mojom::PaintPreviewCompositorCollection> receiver) {
  return std::make_unique<paint_preview::PaintPreviewCompositorCollectionImpl>(
      std::move(receiver), /*initialize_environment=*/true,
      content::UtilityThread::Get()->GetIOTaskRunner());
}
#endif  // BUILDFLAG(ENABLE_PAINT_PREVIEW)

#if BUILDFLAG(ENABLE_OOP_PRINTING)
auto RunPrintingSandboxedPrintBackendHost(
    mojo::PendingReceiver<printing::mojom::SandboxedPrintBackendHost>
        receiver) {
  return std::make_unique<printing::SandboxedPrintBackendHostImpl>(
      std::move(receiver));
}
auto RunPrintingUnsandboxedPrintBackendHost(
    mojo::PendingReceiver<printing::mojom::UnsandboxedPrintBackendHost>
        receiver) {
  return std::make_unique<printing::UnsandboxedPrintBackendHostImpl>(
      std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

#if BUILDFLAG(ENABLE_PRINTING)
auto RunPrintCompositor(
    mojo::PendingReceiver<printing::mojom::PrintCompositor> receiver) {
  return std::make_unique<printing::PrintCompositorImpl>(
      std::move(receiver), true /* initialize_environment */,
      content::UtilityThread::Get()->GetIOTaskRunner());
}
#endif  // BUILDFLAG(ENABLE_PRINTING)

#if BUILDFLAG(IS_CHROMEOS_ASH)
auto RunImeService(
    mojo::PendingReceiver<ash::ime::mojom::ImeService> receiver) {
  return std::make_unique<ash::ime::ImeService>(
      std::move(receiver), ash::ime::ImeSharedLibraryWrapperImpl::GetInstance(),
      std::make_unique<ash::ime::FieldTrialParamsRetrieverImpl>());
}

auto RunOrcaService(
    mojo::PendingReceiver<ash::orca::mojom::OrcaService> receiver) {
  CHECK(chromeos::features::IsOrcaEnabled());
  auto orca_library = std::make_unique<ash::orca::OrcaLibrary>();
  base::expected<void, ash::orca::OrcaLibrary::BindError> error =
      orca_library->BindReceiver(std::move(receiver));
  if (!error.has_value()) {
    LOG(ERROR) << error.error().message;
  }
  return orca_library;
}

auto RunRecordingService(
    mojo::PendingReceiver<recording::mojom::RecordingService> receiver) {
  return std::make_unique<recording::RecordingService>(std::move(receiver));
}

auto RunSharing(mojo::PendingReceiver<sharing::mojom::Sharing> receiver) {
  return std::make_unique<sharing::SharingImpl>(
      std::move(receiver), content::UtilityThread::Get()->GetIOTaskRunner());
}

auto RunTrashService(
    mojo::PendingReceiver<ash::trash_service::mojom::TrashService> receiver) {
  return std::make_unique<ash::trash_service::TrashServiceImpl>(
      std::move(receiver));
}

auto RunTtsService(
    mojo::PendingReceiver<chromeos::tts::mojom::TtsService> receiver) {
  return std::make_unique<chromeos::tts::TtsService>(std::move(receiver));
}

auto RunLocalSearchService(
    mojo::PendingReceiver<ash::local_search_service::mojom::LocalSearchService>
        receiver) {
  return std::make_unique<ash::local_search_service::LocalSearchService>(
      std::move(receiver));
}

auto RunQuickPairService(
    mojo::PendingReceiver<ash::quick_pair::mojom::QuickPairService> receiver) {
  return std::make_unique<ash::quick_pair::QuickPairService>(
      std::move(receiver));
}

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
auto RunAssistantAudioDecoder(
    mojo::PendingReceiver<ash::assistant::mojom::AssistantAudioDecoderFactory>
        receiver) {
  return std::make_unique<ash::assistant::AssistantAudioDecoderFactory>(
      std::move(receiver));
}

auto RunLibassistantService(
    mojo::PendingReceiver<ash::libassistant::mojom::LibassistantService>
        receiver) {
  return std::make_unique<ash::libassistant::LibassistantService>(
      std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
auto RunQuickAnswersSpellCheckService(
    mojo::PendingReceiver<quick_answers::mojom::SpellCheckService> receiver) {
  return std::make_unique<quick_answers::SpellCheckService>(
      std::move(receiver));
}

auto RunMahiContentExtractionServiceFactory(
    mojo::PendingReceiver<mahi::mojom::ContentExtractionServiceFactory>
        receiver) {
  return std::make_unique<mahi::ContentExtractionService>(std::move(receiver));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

auto RunOnDeviceTranslationService(
    mojo::PendingReceiver<
        on_device_translation::mojom::OnDeviceTranslationService> receiver) {
  return std::make_unique<on_device_translation::OnDeviceTranslationService>(
      std::move(receiver));
}

}  // namespace

void RegisterElevatedMainThreadServices(mojo::ServiceFactory& services) {
  // NOTE: This ServiceFactory is only used in utility processes which are run
  // with elevated system privileges.
#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_WIN)
  // On non-Windows, this service runs in a regular utility process.
  services.Add(RunRemovableStorageWriter);
#endif
}

void RegisterMainThreadServices(mojo::ServiceFactory& services) {
  services.Add(RunFilePatcher);
  services.Add(RunUnzipper);
  services.Add(RunLanguageDetectionService);
  services.Add(RunWebAppOriginAssociationParser);
  services.Add(RunCSVPasswordParser);

#if !BUILDFLAG(IS_ANDROID)
  services.Add(RunProfileImporter);
  services.Add(RunMirroringService);
  services.Add(RunPassageEmbeddingsService);
  services.Add(RunScreenAIServiceFactory);
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_BROWSER_SPEECH_SERVICE)
  services.Add(RunSpeechRecognitionService);
#endif  // !BUILDFLAG(ENABLE_BROWSER_SPEECH_SERVICE)

#if BUILDFLAG(IS_WIN)
  services.Add(RunProcessorMetrics);
  services.Add(RunQuarantineService);
  services.Add(RunWindowsUtility);
  services.Add(RunWindowsIconReader);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  services.Add(RunSystemSignalsService);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  services.Add(RunCupsIppParser);
#endif

#if BUILDFLAG(IS_MAC)
  services.Add(RunMacNotificationService);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(FULL_SAFE_BROWSING) || BUILDFLAG(IS_CHROMEOS_ASH)
  services.Add(RunFileUtil);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) && !BUILDFLAG(IS_WIN)
  // On Windows, this service runs in an elevated utility process.
  services.Add(RunRemovableStorageWriter);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) || BUILDFLAG(IS_ANDROID)
  services.Add(RunMediaParserFactory);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  services.Add(RunPdfService);
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_PRINTING) && BUILDFLAG(IS_WIN))
  services.Add(RunPrintingService);
#endif

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  services.Add(RunPrintingSandboxedPrintBackendHost);
  services.Add(RunPrintingUnsandboxedPrintBackendHost);
#endif

#if BUILDFLAG(ENABLE_PRINTING)
  services.Add(RunPrintCompositor);
#endif

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
  services.Add(RunPaintPreviewCompositor);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  services.Add(RunImeService);
  if (chromeos::features::IsOrcaEnabled()) {
    services.Add(RunOrcaService);
  }
  services.Add(RunRecordingService);
  services.Add(RunSharing);
  services.Add(RunTrashService);
  services.Add(RunTtsService);
  services.Add(RunLocalSearchService);
  services.Add(RunQuickPairService);
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  services.Add(RunAssistantAudioDecoder);
  services.Add(RunLibassistantService);
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
  services.Add(RunQuickAnswersSpellCheckService);
  services.Add(RunMahiContentExtractionServiceFactory);
#endif  // BUILDFLAG(IS_CHROMEOS)

  services.Add(RunOnDeviceTranslationService);
}

void RegisterIOThreadServices(mojo::ServiceFactory& services) {
#if !BUILDFLAG(IS_ANDROID)
  services.Add(RunProxyResolver);
#endif
#if BUILDFLAG(IS_WIN)
  services.Add(RunWindowsSystemProxyResolver);
#endif  // BUILDFLAG(IS_WIN)
}
