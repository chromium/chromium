// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/services.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/services/machine_learning/machine_learning_service.h"  // nogncheck
#include "chrome/services/machine_learning/public/mojom/machine_learning_service.mojom.h"  // nogncheck
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"  // nogncheck
#include "chrome/services/qrcode_generator/qrcode_generator_service_impl.h"  // nogncheck
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/safe_browsing/buildflags.h"
#include "components/services/language_detection/language_detection_service_impl.h"
#include "components/services/language_detection/public/mojom/language_detection.mojom.h"
#include "components/services/patch/file_patcher_impl.h"
#include "components/services/patch/public/mojom/file_patcher.mojom.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "components/services/unzip/unzipper_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/utility/utility_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/service_factory.h"
#include "printing/buildflags/buildflags.h"

#if defined(OS_WIN)
#include "chrome/services/util_win/public/mojom/util_read_icon.mojom.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "chrome/services/util_win/util_read_icon.h"
#include "chrome/services/util_win/util_win_impl.h"
#include "components/services/quarantine/public/cpp/quarantine_features_win.h"  // nogncheck
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"  // nogncheck
#include "components/services/quarantine/quarantine_impl.h"  // nogncheck
#endif  // defined(OS_WIN)

#if !defined(OS_ANDROID)
#include "chrome/common/importer/profile_import.mojom.h"
#include "chrome/services/speech/speech_recognition_service_impl.h"
#include "chrome/utility/importer/profile_import_impl.h"
#include "components/mirroring/service/mirroring_service.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "services/proxy_resolver/proxy_resolver_factory_impl.h"  // nogncheck
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_PRINTING) && defined(OS_CHROMEOS)
#include "chrome/services/ipp_parser/ipp_parser.h"  // nogncheck
#include "chrome/services/ipp_parser/public/mojom/ipp_parser.mojom.h"  // nogncheck
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING) || defined(OS_CHROMEOS)
#include "chrome/services/file_util/file_util_service.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/services/removable_storage_writer/public/mojom/removable_storage_writer.mojom.h"
#include "chrome/services/removable_storage_writer/removable_storage_writer.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_ANDROID)
#include "chrome/services/media_gallery_util/media_parser_factory.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_PRINTING) && defined(OS_WIN))
#include "chrome/services/printing/printing_service.h"
#include "chrome/services/printing/public/mojom/printing_service.mojom.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/services/print_compositor/print_compositor_impl.h"  // nogncheck
#include "components/services/print_compositor/public/mojom/print_compositor.mojom.h"  // nogncheck
#endif

#include "components/services/paint_preview_compositor/paint_preview_compositor_collection_impl.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"

#if defined(OS_CHROMEOS)
#include "chrome/services/sharing/public/mojom/sharing.mojom.h"  // nogncheck
#include "chrome/services/sharing/sharing_impl.h"
#include "chromeos/assistant/buildflags.h"  // nogncheck
#include "chromeos/services/ime/ime_service.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "chromeos/services/tts/tts_service.h"

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "chromeos/services/assistant/audio_decoder/assistant_audio_decoder_factory.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // defined(OS_CHROMEOS)

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

auto RunQRCodeGeneratorService(
    mojo::PendingReceiver<qrcode_generator::mojom::QRCodeGeneratorService>
        receiver) {
  return std::make_unique<qrcode_generator::QRCodeGeneratorServiceImpl>(
      std::move(receiver));
}

auto RunMachineLearningService(
    mojo::PendingReceiver<machine_learning::mojom::MachineLearningService>
        receiver) {
  return std::make_unique<machine_learning::MachineLearningService>(
      std::move(receiver));
}

#if defined(OS_WIN)
auto RunQuarantineService(
    mojo::PendingReceiver<quarantine::mojom::Quarantine> receiver) {
  DCHECK(base::FeatureList::IsEnabled(quarantine::kOutOfProcessQuarantine));
  return std::make_unique<quarantine::QuarantineImpl>(std::move(receiver));
}

auto RunWindowsUtility(mojo::PendingReceiver<chrome::mojom::UtilWin> receiver) {
  return std::make_unique<UtilWinImpl>(std::move(receiver));
}

auto RunWindowsIconReader(
    mojo::PendingReceiver<chrome::mojom::UtilReadIcon> receiver) {
  return std::make_unique<UtilReadIcon>(std::move(receiver));
}
#endif  // defined(OS_WIN)

#if !defined(OS_ANDROID)
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

auto RunSpeechRecognitionService(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionService> receiver) {
  return std::make_unique<speech::SpeechRecognitionServiceImpl>(
      std::move(receiver));
}
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_PRINTING) && defined(OS_CHROMEOS)
auto RunCupsIppParser(
    mojo::PendingReceiver<ipp_parser::mojom::IppParser> receiver) {
  return std::make_unique<ipp_parser::IppParser>(std::move(receiver));
}
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING) || defined(OS_CHROMEOS)
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

#if BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_ANDROID)
auto RunMediaParserFactory(
    mojo::PendingReceiver<chrome::mojom::MediaParserFactory> receiver) {
  return std::make_unique<MediaParserFactory>(std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_PRINTING) && defined(OS_WIN))
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

#if BUILDFLAG(ENABLE_PRINTING)
auto RunPrintCompositor(
    mojo::PendingReceiver<printing::mojom::PrintCompositor> receiver) {
  return std::make_unique<printing::PrintCompositorImpl>(
      std::move(receiver), true /* initialize_environment */,
      content::UtilityThread::Get()->GetIOTaskRunner());
}
#endif

#if defined(OS_CHROMEOS)
auto RunImeService(
    mojo::PendingReceiver<chromeos::ime::mojom::ImeService> receiver) {
  return std::make_unique<chromeos::ime::ImeService>(std::move(receiver));
}

auto RunSharing(mojo::PendingReceiver<sharing::mojom::Sharing> receiver) {
  return std::make_unique<sharing::SharingImpl>(
      std::move(receiver), content::UtilityThread::Get()->GetIOTaskRunner());
}

auto RunTtsService(
    mojo::PendingReceiver<chromeos::tts::mojom::TtsService> receiver) {
  return std::make_unique<chromeos::tts::TtsService>(std::move(receiver));
}

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
auto RunAssistantAudioDecoder(
    mojo::PendingReceiver<
        chromeos::assistant::mojom::AssistantAudioDecoderFactory> receiver) {
  return std::make_unique<chromeos::assistant::AssistantAudioDecoderFactory>(
      std::move(receiver));
}
#endif
#endif

}  // namespace

mojo::ServiceFactory* GetElevatedMainThreadServiceFactory() {
  // NOTE: This ServiceFactory is only used in utility processes which are run
  // with elevated system privileges.
  // clang-format off
  static base::NoDestructor<mojo::ServiceFactory> factory {
#if BUILDFLAG(ENABLE_EXTENSIONS) && defined(OS_WIN)
    // On non-Windows, this service runs in a regular utility process.
    RunRemovableStorageWriter,
#endif
  };
  // clang-format on
  return factory.get();
}

mojo::ServiceFactory* GetMainThreadServiceFactory() {
  // clang-format off
  static base::NoDestructor<mojo::ServiceFactory> factory {
    RunFilePatcher,
    RunUnzipper,
    RunLanguageDetectionService,
    RunQRCodeGeneratorService,
    RunMachineLearningService,

#if !defined(OS_ANDROID)
    RunProfileImporter,
    RunMirroringService,
    RunSpeechRecognitionService,
#endif

#if defined(OS_WIN)
    RunQuarantineService,
    RunWindowsUtility,
    RunWindowsIconReader,
#endif  // defined(OS_WIN)

#if BUILDFLAG(ENABLE_PRINTING) && defined(OS_CHROMEOS)
    RunCupsIppParser,
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING) || defined(OS_CHROMEOS)
    RunFileUtil,
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) && !defined(OS_WIN)
    // On Windows, this service runs in an elevated utility process.
    RunRemovableStorageWriter,
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_ANDROID)
    RunMediaParserFactory,
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_PRINTING) && defined(OS_WIN))
    RunPrintingService,
#endif

#if BUILDFLAG(ENABLE_PRINTING)
    RunPrintCompositor,
#endif

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
    RunPaintPreviewCompositor,
#endif

#if defined(OS_CHROMEOS)
    RunImeService,
    RunSharing,
    RunTtsService,
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
    RunAssistantAudioDecoder,
#endif
#endif
  };
  // clang-format on
  return factory.get();
}

mojo::ServiceFactory* GetIOThreadServiceFactory() {
  // clang-format off
  static base::NoDestructor<mojo::ServiceFactory> factory {
#if !defined(OS_ANDROID)
    RunProxyResolver,
#endif  // !defined(OS_ANDROID)
  };
  // clang-format on
  return factory.get();
}
