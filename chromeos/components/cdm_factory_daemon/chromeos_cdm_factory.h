// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CHROMEOS_CDM_FACTORY_H_
#define CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CHROMEOS_CDM_FACTORY_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_context.h"
#include "chromeos/components/cdm_factory_daemon/mojom/browser_cdm_factory.mojom.h"
#include "chromeos/components/cdm_factory_daemon/mojom/cdm_factory_daemon.mojom.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_factory.h"
#include "media/mojo/mojom/cdm_document_service.mojom.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

// Provides an implementation of the media::CdmFactory interface which utilizes
// the chromeos::CdmFactoryDaemonProxy to establish a mojo connection to the
// CDM factory daemon in Chrome OS which it then uses to create a CDM
// implementation. The implementation will be used in the GPU process.
class COMPONENT_EXPORT(CDM_FACTORY_DAEMON) ChromeOsCdmFactory
    : public media::CdmFactory {
 public:
  explicit ChromeOsCdmFactory(
      media::mojom::FrameInterfaceFactory* frame_interfaces);

  ChromeOsCdmFactory(const ChromeOsCdmFactory&) = delete;
  ChromeOsCdmFactory& operator=(const ChromeOsCdmFactory&) = delete;

  ~ChromeOsCdmFactory() override;

  // Invoked on GPU initialization to set the receiver to pass to the browser
  // process.
  static mojo::PendingReceiver<cdm::mojom::BrowserCdmFactory>
  GetBrowserCdmFactoryReceiver();

  // media::CdmFactory implementation.
  void Create(
      const media::CdmConfig& cdm_config,
      const media::SessionMessageCB& session_message_cb,
      const media::SessionClosedCB& session_closed_cb,
      const media::SessionKeysChangeCB& session_keys_change_cb,
      const media::SessionExpirationUpdateCB& session_expiration_update_cb,
      media::CdmCreatedCB cdm_created_cb) override;

  // Used to get hardware specific configuration data from the daemon to be used
  // for setting up decrypt+decode in the GPU.
  static void GetHwConfigData(ChromeOsCdmContext::GetHwConfigDataCB callback);

  // Used to get screen resolutions from the browser process so we can optimize
  // our decode target size.
  static void GetScreenResolutions(
      ChromeOsCdmContext::GetScreenResolutionsCB callback);

  // Allocates a secure buffer with ARM TrustZone for the target of decryption.
  static void AllocateSecureBuffer(
      uint32_t size,
      ChromeOsCdmContext::AllocateSecureBufferCB callback);

  // Parses H264 slice header data referenced in TrustZone memory by
  // |secure_handle|.
  static void ParseEncryptedSliceHeader(
      uint64_t secure_handle,
      uint32_t offset,
      const std::vector<uint8_t>& stream_data,
      ChromeOsCdmContext::ParseEncryptedSliceHeaderCB callback);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Invoked in the OOP Video Decoder utility process to set the Mojo connection
  // back to the browser process. Normally the GPU process has a Mojo connection
  // to the browser process for this purpose. When we launch an OOP Video
  // Decoder Process from the browser process, we pass a mojo::Remote to it so
  // the OOP Video Decoder process can then invoke cdm::mojom::BrowserCdmFactory
  // calls that are received by the browser process just like they would if it
  // was being handled in the GPU process.
  static void SetBrowserCdmFactoryRemote(
      mojo::Remote<cdm::mojom::BrowserCdmFactory> remote);

  // Returns a singleton pointer that can be used as the media::CdmContext for
  // ARC video decode operations.
  static media::CdmContext* GetArcCdmContext();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  void OnVerifiedAccessEnabled(
      const media::CdmConfig& cdm_config,
      const media::SessionMessageCB& session_message_cb,
      const media::SessionClosedCB& session_closed_cb,
      const media::SessionKeysChangeCB& session_keys_change_cb,
      const media::SessionExpirationUpdateCB& session_expiration_update_cb,
      media::CdmCreatedCB cdm_created_cb,
      bool enabled);
  void OnCreateFactory(
      const media::CdmConfig& cdm_config,
      const media::SessionMessageCB& session_message_cb,
      const media::SessionClosedCB& session_closed_cb,
      const media::SessionKeysChangeCB& session_keys_change_cb,
      const media::SessionExpirationUpdateCB& session_expiration_update_cb,
      media::CdmCreatedCB cdm_created_cb,
      mojo::PendingRemote<cdm::mojom::CdmFactory> remote_factory);
  void CreateCdm(
      const media::CdmConfig& cdm_config,
      const media::SessionMessageCB& session_message_cb,
      const media::SessionClosedCB& session_closed_cb,
      const media::SessionKeysChangeCB& session_keys_change_cb,
      const media::SessionExpirationUpdateCB& session_expiration_update_cb,
      media::CdmCreatedCB cdm_created_cb);
  void OnFactoryMojoConnectionError();
  void OnVerificationMojoConnectionError();

  raw_ptr<media::mojom::FrameInterfaceFactory> frame_interfaces_;
  mojo::Remote<cdm::mojom::CdmFactory> remote_factory_;
  mojo::Remote<media::mojom::CdmDocumentService> cdm_document_service_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ChromeOsCdmFactory> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CHROMEOS_CDM_FACTORY_H_
