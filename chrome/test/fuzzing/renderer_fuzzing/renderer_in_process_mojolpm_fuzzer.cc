// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/fuzzing/renderer_fuzzing/renderer_in_process_mojolpm_fuzzer.h"

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/escape.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_proto_fuzzer.h"
#include "chrome/test/fuzzing/renderer_fuzzing/in_process_renderer_fuzzing.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/tools/fuzzers/mojolpm.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "testing/libfuzzer/renderer_fuzzing/renderer_fuzzing.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-mojolpm.h"
#include "third_party/blink/public/web/web_testing_support.h"

// This class fuzzes mojo interfaces exposed by the browser process to the
// renderer process using MojoLPM.
// It runs in the renderer process, and is fed testcases by the fuzzer
// running in the browser process.
// It currently uses MojoLPMGenerator in order to remove all the unnecessary
// boilerplate implied by using MojoLPM.
class RendererTestcase
    : public mojolpmgenerator::RendererInProcessMojolpmFuzzerTestcase {
 public:
  explicit RendererTestcase(
      std::unique_ptr<ProtoTestcase> testcase,
      const blink::BrowserInterfaceBrokerProxy* context_interface_broker_proxy,
      blink::ThreadSafeBrowserInterfaceBrokerProxy*
          process_interface_broker_proxy);
  ~RendererTestcase() override;

  scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunner() override;
  void SetUp(base::OnceClosure done_closure) override;
  void TearDown(base::OnceClosure done_closure) override;
  void HandleNewBlobRegistryAction(uint32_t id,
                                   base::OnceClosure done_closure) override;
  // At some point, this will be automatically generated. As for now, we want
  // to try fuzzing a larger part of the IPC surface.
  void HandleNewAdAuctionServiceAction(uint32_t id,
                                       base::OnceClosure done_closure) override;
  void HandleNewAnchorElementInteractionHostAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewAnchorElementMetricsHostAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewAudioContextManagerAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewAuthenticatorAction(uint32_t id,
                                    base::OnceClosure done_closure) override;
  void HandleNewBackgroundFetchServiceAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewBlobURLStoreAction(uint32_t id,
                                   base::OnceClosure done_closure) override;
  void HandleNewBrowsingTopicsDocumentServiceAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewBucketManagerHostAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewCacheStorageAction(uint32_t id,
                                   base::OnceClosure done_closure) override;
  void HandleNewClipboardHostAction(uint32_t id,
                                    base::OnceClosure done_closure) override;
  void HandleNewCodeCacheHostAction(uint32_t id,
                                    base::OnceClosure done_closure) override;
  void HandleNewColorChooserFactoryAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewContactsManagerAction(uint32_t id,
                                      base::OnceClosure done_closure) override;
  void HandleNewContentIndexServiceAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewContentSecurityNotifierAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewCookieStoreAction(uint32_t id,
                                  base::OnceClosure done_closure) override;
  void HandleNewCredentialManagerAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewDedicatedWorkerHostFactoryAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewDeviceAPIServiceAction(uint32_t id,
                                       base::OnceClosure done_closure) override;
  void HandleNewDevicePostureProviderAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewDigitalIdentityRequestAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewDirectSocketsServiceAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewEyeDropperChooserAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewFeatureObserverAction(uint32_t id,
                                      base::OnceClosure done_closure) override;
  void HandleNewFederatedAuthRequestAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewFileChooserAction(uint32_t id,
                                  base::OnceClosure done_closure) override;
  void HandleNewFileSystemAccessManagerAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewFileSystemManagerAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewFileUtilitiesHostAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewFontAccessManagerAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewGeolocationServiceAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewHidServiceAction(uint32_t id,
                                 base::OnceClosure done_closure) override;
  void HandleNewIDBFactoryAction(uint32_t id,
                                 base::OnceClosure done_closure) override;
  void HandleNewIdleManagerAction(uint32_t id,
                                  base::OnceClosure done_closure) override;
  void HandleNewInstalledAppProviderAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewKeyboardLockServiceAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewLCPCriticalPathPredictorHostAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewLockManagerAction(uint32_t id,
                                  base::OnceClosure done_closure) override;
  void HandleNewManagedConfigurationServiceAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewMediaDevicesDispatcherHostAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewMediaSessionServiceAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewMediaStreamDispatcherHostAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewModelManagerAction(uint32_t id,
                                   base::OnceClosure done_closure) override;
  void HandleNewNonAssociatedLocalFrameHostAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewNoStatePrefetchProcessorAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewNotificationServiceAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewOneShotBackgroundSyncServiceAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewOriginTrialStateHostAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewPeerConnectionTrackerHostAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewPeriodicBackgroundSyncServiceAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewPermissionServiceAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewPictureInPictureServiceAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewPresentationServiceAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewPushMessagingAction(uint32_t id,
                                    base::OnceClosure done_closure) override;
  void HandleNewQuotaManagerHostAction(uint32_t id,
                                       base::OnceClosure done_closure) override;
  void HandleNewRenderAccessibilityHostAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewRendererAudioInputStreamFactoryAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewRendererAudioOutputStreamFactoryAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewReportingServiceProxyAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewSerialServiceAction(uint32_t id,
                                    base::OnceClosure done_closure) override;
  void HandleNewSharedWorkerConnectorAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewSpeculationHostAction(uint32_t id,
                                      base::OnceClosure done_closure) override;
  void HandleNewSpeechRecognizerAction(uint32_t id,
                                       base::OnceClosure done_closure) override;
  void HandleNewSpeechSynthesisAction(uint32_t id,
                                      base::OnceClosure done_closure) override;
  void HandleNewStorageAccessHandleAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewTextSuggestionHostAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewWakeLockServiceAction(uint32_t id,
                                      base::OnceClosure done_closure) override;
  void HandleNewWebBluetoothServiceAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewWebOTPServiceAction(uint32_t id,
                                    base::OnceClosure done_closure) override;
  void HandleNewWebSensorProviderAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewWebSocketConnectorAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewWebTransportConnectorAction(
      uint32_t id,
      base::OnceClosure done_closure) override;
  void HandleNewWebUsbServiceAction(uint32_t id,
                                    base::OnceClosure done_closure) override;

 private:
  void SetUpOnFuzzerThread(base::OnceClosure done_closure);
  void TearDownOnFuzzerThread(base::OnceClosure done_closure);

  template <typename T>
  void NewProcessInterface(uint32_t id, base::OnceClosure done_closure);
  template <typename T>
  void NewContextInterface(uint32_t id, base::OnceClosure done_closure);

  // This is different to the "normal" MojoLPM testcase model, since we need
  // to also own the lifetime of the protobuf object, when it's normally owned
  // by libfuzzer.
  std::unique_ptr<ProtoTestcase> proto_testcase_ptr_;

  // Bindings
  [[maybe_unused]] raw_ptr<const blink::BrowserInterfaceBrokerProxy>
      context_interface_broker_proxy_;
  [[maybe_unused]] raw_ptr<blink::ThreadSafeBrowserInterfaceBrokerProxy>
      process_interface_broker_proxy_;

  SEQUENCE_CHECKER(sequence_checker_);
};

namespace {

scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunnerImpl() {
  // XXX: This should be main thread? IO thread? Probably doesn't
  // actually matter.
  static scoped_refptr<base::SequencedTaskRunner> fuzzer_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  return fuzzer_task_runner;
}

}  // anonymous namespace

void RendererTestcase::HandleNewBlobRegistryAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewProcessInterface<::blink::mojom::BlobRegistry>(id,
                                                    std::move(done_closure));
}

void RendererTestcase::HandleNewAdAuctionServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::AdAuctionService>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewAnchorElementInteractionHostAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::AnchorElementInteractionHost>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewAnchorElementMetricsHostAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::AnchorElementMetricsHost>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewAudioContextManagerAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::AudioContextManager>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewAuthenticatorAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::Authenticator>(id,
                                                     std::move(done_closure));
}

void RendererTestcase::HandleNewBackgroundFetchServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::BackgroundFetchService>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewBlobURLStoreAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::BlobURLStore>(id,
                                                    std::move(done_closure));
}

void RendererTestcase::HandleNewBrowsingTopicsDocumentServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::BrowsingTopicsDocumentService>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewBucketManagerHostAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::BucketManagerHost>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewCacheStorageAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::CacheStorage>(id,
                                                    std::move(done_closure));
}

void RendererTestcase::HandleNewClipboardHostAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::ClipboardHost>(id,
                                                     std::move(done_closure));
}

void RendererTestcase::HandleNewCodeCacheHostAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::CodeCacheHost>(id,
                                                     std::move(done_closure));
}

void RendererTestcase::HandleNewColorChooserFactoryAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::ColorChooserFactory>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewContactsManagerAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::ContactsManager>(id,
                                                       std::move(done_closure));
}

void RendererTestcase::HandleNewContentIndexServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::ContentIndexService>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewContentSecurityNotifierAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::ContentSecurityNotifier>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewCookieStoreAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::CookieStore>(id, std::move(done_closure));
}

void RendererTestcase::HandleNewCredentialManagerAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::CredentialManager>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewDedicatedWorkerHostFactoryAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::DedicatedWorkerHostFactory>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewDeviceAPIServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::DeviceAPIService>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewDevicePostureProviderAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::DevicePostureProvider>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewDigitalIdentityRequestAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::DigitalIdentityRequest>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewDirectSocketsServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::DirectSocketsService>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewEyeDropperChooserAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::EyeDropperChooser>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewFeatureObserverAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::FeatureObserver>(id,
                                                       std::move(done_closure));
}

void RendererTestcase::HandleNewFederatedAuthRequestAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::FederatedAuthRequest>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewFileChooserAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::FileChooser>(id, std::move(done_closure));
}

void RendererTestcase::HandleNewFileSystemAccessManagerAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::FileSystemAccessManager>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewFileSystemManagerAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::FileSystemManager>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewFileUtilitiesHostAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::FileUtilitiesHost>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewFontAccessManagerAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::FontAccessManager>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewGeolocationServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::GeolocationService>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewHidServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::HidService>(id, std::move(done_closure));
}

void RendererTestcase::HandleNewIDBFactoryAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::IDBFactory>(id, std::move(done_closure));
}

void RendererTestcase::HandleNewIdleManagerAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::IdleManager>(id, std::move(done_closure));
}

void RendererTestcase::HandleNewInstalledAppProviderAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::InstalledAppProvider>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewKeyboardLockServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::KeyboardLockService>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewLockManagerAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::LockManager>(id, std::move(done_closure));
}

void RendererTestcase::HandleNewLCPCriticalPathPredictorHostAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::LCPCriticalPathPredictorHost>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewManagedConfigurationServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::ManagedConfigurationService>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewMediaDevicesDispatcherHostAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::MediaDevicesDispatcherHost>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewMediaSessionServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::MediaSessionService>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewMediaStreamDispatcherHostAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::MediaStreamDispatcherHost>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewModelManagerAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::ModelManager>(id,
                                                    std::move(done_closure));
}

void RendererTestcase::HandleNewNonAssociatedLocalFrameHostAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::NonAssociatedLocalFrameHost>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewNoStatePrefetchProcessorAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::NoStatePrefetchProcessor>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewNotificationServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::NotificationService>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewOneShotBackgroundSyncServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::OneShotBackgroundSyncService>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewOriginTrialStateHostAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::OriginTrialStateHost>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewPeerConnectionTrackerHostAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::PeerConnectionTrackerHost>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewPeriodicBackgroundSyncServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::PeriodicBackgroundSyncService>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewPermissionServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::PermissionService>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewPictureInPictureServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::PictureInPictureService>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewPresentationServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::PresentationService>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewPushMessagingAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::PushMessaging>(id,
                                                     std::move(done_closure));
}

void RendererTestcase::HandleNewQuotaManagerHostAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::QuotaManagerHost>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewRenderAccessibilityHostAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::RenderAccessibilityHost>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewRendererAudioInputStreamFactoryAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::RendererAudioInputStreamFactory>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewRendererAudioOutputStreamFactoryAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::RendererAudioOutputStreamFactory>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewReportingServiceProxyAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::ReportingServiceProxy>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewSerialServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::SerialService>(id,
                                                     std::move(done_closure));
}

void RendererTestcase::HandleNewSharedWorkerConnectorAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::SharedWorkerConnector>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewSpeculationHostAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::SpeculationHost>(id,
                                                       std::move(done_closure));
}

void RendererTestcase::HandleNewSpeechRecognizerAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::SpeechRecognizer>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewSpeechSynthesisAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::SpeechSynthesis>(id,
                                                       std::move(done_closure));
}

void RendererTestcase::HandleNewStorageAccessHandleAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::StorageAccessHandle>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewTextSuggestionHostAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::TextSuggestionHost>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewWakeLockServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::WakeLockService>(id,
                                                       std::move(done_closure));
}

void RendererTestcase::HandleNewWebBluetoothServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::WebBluetoothService>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewWebOTPServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::WebOTPService>(id,
                                                     std::move(done_closure));
}

void RendererTestcase::HandleNewWebSensorProviderAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::WebSensorProvider>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewWebSocketConnectorAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::WebSocketConnector>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewWebTransportConnectorAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::WebTransportConnector>(
      id, std::move(done_closure));
}

void RendererTestcase::HandleNewWebUsbServiceAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewContextInterface<::blink::mojom::WebUsbService>(id,
                                                     std::move(done_closure));
}

scoped_refptr<base::SequencedTaskRunner>
RendererTestcase::GetFuzzerTaskRunner() {
  return GetFuzzerTaskRunnerImpl();
}

RendererTestcase::RendererTestcase(
    std::unique_ptr<ProtoTestcase> testcase,
    const blink::BrowserInterfaceBrokerProxy* context_interface_broker_proxy,
    blink::ThreadSafeBrowserInterfaceBrokerProxy*
        process_interface_broker_proxy)
    : mojolpmgenerator::RendererInProcessMojolpmFuzzerTestcase(*testcase.get()),
      proto_testcase_ptr_(std::move(testcase)),
      context_interface_broker_proxy_(context_interface_broker_proxy),
      process_interface_broker_proxy_(process_interface_broker_proxy) {
  // RendererTestcase is created on the main thread, but the actions that
  // we want to validate the sequencing of take place on the fuzzer sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RendererTestcase::~RendererTestcase() {}

void RendererTestcase::SetUp(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RendererTestcase::SetUpOnFuzzerThread,
                     base::Unretained(this), std::move(done_closure)));
}

void RendererTestcase::TearDown(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RendererTestcase::TearDownOnFuzzerThread,
                     base::Unretained(this), std::move(done_closure)));
}

void RendererTestcase::SetUpOnFuzzerThread(base::OnceClosure done_closure) {
  mojolpm::GetContext()->StartTestcase();

  std::move(done_closure).Run();
}

void RendererTestcase::TearDownOnFuzzerThread(base::OnceClosure done_closure) {
  mojolpm::GetContext()->EndTestcase();

  std::move(done_closure).Run();
}

template <typename T>
void RendererTestcase::NewProcessInterface(uint32_t id,
                                           base::OnceClosure done_closure) {
  mojo::Remote<T> remote;
  mojo::GenericPendingReceiver receiver = remote.BindNewPipeAndPassReceiver();

  process_interface_broker_proxy_->GetInterface(std::move(receiver));
  CHECK(remote.is_bound() && remote.is_connected());

  mojolpm::GetContext()->AddInstance(id, std::move(remote));

  std::move(done_closure).Run();
}

template <typename T>
void RendererTestcase::NewContextInterface(uint32_t id,
                                           base::OnceClosure done_closure) {
  mojo::Remote<T> remote;
  mojo::GenericPendingReceiver receiver = remote.BindNewPipeAndPassReceiver();

  context_interface_broker_proxy_->GetInterface(std::move(receiver));
  CHECK(remote.is_bound() && remote.is_connected());

  mojolpm::GetContext()->AddInstance(id, std::move(remote));

  std::move(done_closure).Run();
}

// `RendererFuzzingAdapter` will be allocated by the internal renderer fuzzing
// mechanism. It is statically allocated, and will remain alive until the
// fuzzing process shuts down.
// Unfortunately, we cannot merge this class with `RendererTestcase`, because
// the latter needs to have a different lifetime. Indeed, it needs to be
// recreated for every fuzzing iteration, so that MojoLPM remains deterministic
// across runs for a given testcase.
class RendererFuzzingAdapter : public RendererFuzzerBase {
 public:
  using FuzzCase = RendererTestcase::ProtoTestcase;
  const char* Id() override { return "MojoLPMRendererFuzzer"; }
  void Run(
      const blink::BrowserInterfaceBrokerProxy* context_interface_broker_proxy,
      blink::ThreadSafeBrowserInterfaceBrokerProxy*
          process_interface_broker_proxy,
      std::vector<uint8_t>&& input,
      base::OnceClosure done_closure) override {
    auto proto_testcase_ptr =
        std::make_unique<RendererTestcase::ProtoTestcase>();
    if (proto_testcase_ptr->ParseFromArray(input.data(), input.size())) {
      auto ptr = std::make_unique<RendererTestcase>(
          std::move(proto_testcase_ptr), context_interface_broker_proxy,
          process_interface_broker_proxy);

      GetFuzzerTaskRunnerImpl()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &mojolpm::RunTestcase<RendererTestcase>,
              base::Unretained(ptr.get()), GetFuzzerTaskRunnerImpl(),
              std::move(done_closure)
                  .Then(base::OnceClosure(
                      base::DoNothingWithBoundArgs(std::move(ptr))))));
    } else {
      std::move(done_closure).Run();
    }
  }
};

REGISTER_IN_PROCESS_RENDERER_PROTO_FUZZER(RendererFuzzingAdapter);
