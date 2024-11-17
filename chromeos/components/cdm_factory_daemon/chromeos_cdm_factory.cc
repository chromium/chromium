// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_factory.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/components/cdm_factory_daemon/cdm_storage_adapter.h"
#include "chromeos/components/cdm_factory_daemon/content_decryption_module_adapter.h"
#include "chromeos/components/cdm_factory_daemon/mojom/content_decryption_module.mojom.h"
#include "media/base/content_decryption_module.h"
#include "media/base/decrypt_config.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace chromeos {

namespace {

// This class is used as a singleton which then allows replacing the underlying
// mojo::Remote<cdm::mojom::BrowserCdmFactory> that it will return.
class BrowserCdmFactoryRemoteHolder {
 public:
  BrowserCdmFactoryRemoteHolder() = default;
  BrowserCdmFactoryRemoteHolder(const BrowserCdmFactoryRemoteHolder&) = delete;
  BrowserCdmFactoryRemoteHolder& operator=(
      const BrowserCdmFactoryRemoteHolder&) = delete;
  ~BrowserCdmFactoryRemoteHolder() = default;

  mojo::Remote<cdm::mojom::BrowserCdmFactory>& GetBrowserCdmFactoryRemote() {
    return remote_;
  }

  void ReplaceBrowserCdmFactoryRemote(
      mojo::Remote<cdm::mojom::BrowserCdmFactory> remote) {
    remote_ = std::move(remote);
  }

 private:
  mojo::Remote<cdm::mojom::BrowserCdmFactory> remote_;
};

// This holds the global single BrowserCdmFactoryRemoteHolder object which then
// internally can hold either the Mojo connection to the browser process or the
// Mojo connection to the GPU process if we are doing OOP video decoding.
BrowserCdmFactoryRemoteHolder& GetBrowserCdmFactoryRemoteHolder() {
  static base::NoDestructor<BrowserCdmFactoryRemoteHolder> remote_holder;
  return *remote_holder;
}

// This holds the global singleton Mojo connection to the browser process.
mojo::Remote<cdm::mojom::BrowserCdmFactory>& GetBrowserCdmFactoryRemote() {
  return GetBrowserCdmFactoryRemoteHolder().GetBrowserCdmFactoryRemote();
}

// This holds the task runner we are bound to.
scoped_refptr<base::SequencedTaskRunner>& GetFactoryTaskRunner() {
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>> runner;
  return *runner;
}

void CreateFactoryOnTaskRunner(
    const std::string& key_system,
    cdm::mojom::BrowserCdmFactory::CreateFactoryCallback callback) {
  GetBrowserCdmFactoryRemote()->CreateFactory(key_system, std::move(callback));
}

void GetOutputProtectionOnTaskRunner(
    mojo::PendingReceiver<cdm::mojom::OutputProtection> output_protection) {
  GetBrowserCdmFactoryRemote()->GetOutputProtection(
      std::move(output_protection));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class SingletonCdmContextRef : public media::CdmContextRef {
 public:
  explicit SingletonCdmContextRef(media::CdmContext* cdm_context)
      : cdm_context_(cdm_context) {}
  ~SingletonCdmContextRef() override = default;

  // media::CdmContextRef
  media::CdmContext* GetCdmContext() override {
    // Safe because we are a singleton for the process.
    return cdm_context_;
  }

 private:
  raw_ptr<media::CdmContext> cdm_context_;
};

void GetHwKeyDataProxy(const std::string& key_id,
                       const std::vector<uint8_t>& hw_identifier,
                       ChromeOsCdmContext::GetHwKeyDataCB callback) {
  if (!GetFactoryTaskRunner()->RunsTasksInCurrentSequence()) {
    GetFactoryTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&GetHwKeyDataProxy, key_id, hw_identifier,
                                  std::move(callback)));
    return;
  }
  GetBrowserCdmFactoryRemote()->GetAndroidHwKeyData(
      std::vector<uint8_t>(key_id.begin(), key_id.end()), hw_identifier,
      std::move(callback));
}

class ArcCdmContext : public ChromeOsCdmContext, public media::CdmContext {
 public:
  ArcCdmContext() = default;
  ArcCdmContext(const ArcCdmContext&) = delete;
  ArcCdmContext& operator=(const ArcCdmContext&) = delete;
  ~ArcCdmContext() override = default;

  // ChromeOsCdmContext implementation.
  void GetHwKeyData(const media::DecryptConfig* decrypt_config,
                    const std::vector<uint8_t>& hw_identifier,
                    GetHwKeyDataCB callback) override {
    GetHwKeyDataProxy(decrypt_config->key_id(), hw_identifier,
                      std::move(callback));
  }
  void GetHwConfigData(
      ChromeOsCdmContext::GetHwConfigDataCB callback) override {
    ChromeOsCdmFactory::GetHwConfigData(std::move(callback));
  }
  void GetScreenResolutions(
      ChromeOsCdmContext::GetScreenResolutionsCB callback) override {
    ChromeOsCdmFactory::GetScreenResolutions(std::move(callback));
  }
  std::unique_ptr<media::CdmContextRef> GetCdmContextRef() override {
    return std::make_unique<SingletonCdmContextRef>(this);
  }
  bool UsingArcCdm() const override { return true; }
  bool IsRemoteCdm() const override { return true; }
  void AllocateSecureBuffer(uint32_t size,
                            AllocateSecureBufferCB callback) override {
    ChromeOsCdmFactory::AllocateSecureBuffer(size, std::move(callback));
  }
  void ParseEncryptedSliceHeader(
      uint64_t secure_handle,
      uint32_t offset,
      const std::vector<uint8_t>& stream_data,
      ParseEncryptedSliceHeaderCB callback) override {
    ChromeOsCdmFactory::ParseEncryptedSliceHeader(
        secure_handle, offset, stream_data, std::move(callback));
  }

  // media::CdmContext implementation.
  ChromeOsCdmContext* GetChromeOsCdmContext() override { return this; }
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void OnCdmCreated(media::CdmCreatedCB callback,
                  scoped_refptr<ContentDecryptionModuleAdapter> cdm,
                  cdm::mojom::CdmFactory::CreateCdmStatus result) {
  switch (result) {
    case cdm::mojom::CdmFactory::CreateCdmStatus::kSuccess:
      std::move(callback).Run(std::move(cdm), media::CreateCdmStatus::kSuccess);
      return;
    case cdm::mojom::CdmFactory::CreateCdmStatus::kNoMoreInstances:
      std::move(callback).Run(nullptr,
                              media::CreateCdmStatus::kNoMoreInstances);
      return;
    case cdm::mojom::CdmFactory::CreateCdmStatus::kInsufficientGpuResources:
      std::move(callback).Run(
          nullptr, media::CreateCdmStatus::kInsufficientGpuResources);
      return;
  }
  std::move(callback).Run(nullptr, media::CreateCdmStatus::kUnknownError);
}
}  // namespace

ChromeOsCdmFactory::ChromeOsCdmFactory(
    media::mojom::FrameInterfaceFactory* frame_interfaces)
    : frame_interfaces_(frame_interfaces) {
  DCHECK(frame_interfaces_);
  DVLOG(1) << "Creating the ChromeOsCdmFactory";
}

ChromeOsCdmFactory::~ChromeOsCdmFactory() = default;

// static
mojo::PendingReceiver<cdm::mojom::BrowserCdmFactory>
ChromeOsCdmFactory::GetBrowserCdmFactoryReceiver() {
  mojo::PendingRemote<chromeos::cdm::mojom::BrowserCdmFactory> browser_proxy;
  auto receiver = browser_proxy.InitWithNewPipeAndPassReceiver();
  GetBrowserCdmFactoryRemote().Bind(std::move(browser_proxy));

  GetFactoryTaskRunner() = base::SequencedTaskRunner::GetCurrentDefault();
  return receiver;
}

void ChromeOsCdmFactory::Create(
    const media::CdmConfig& cdm_config,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionKeysChangeCB& session_keys_change_cb,
    const media::SessionExpirationUpdateCB& session_expiration_update_cb,
    media::CdmCreatedCB cdm_created_cb) {
  DVLOG(1) << __func__ << " cdm_config=" << cdm_config;
  // Check that the user has Verified Access enabled in their Chrome settings
  // and if they do not then block this connection since OEMCrypto utilizes
  // remote attestation as part of verification.
  if (!cdm_document_service_) {
    frame_interfaces_->BindEmbedderReceiver(mojo::GenericPendingReceiver(
        cdm_document_service_.BindNewPipeAndPassReceiver()));
    cdm_document_service_.set_disconnect_handler(
        base::BindOnce(&ChromeOsCdmFactory::OnVerificationMojoConnectionError,
                       weak_factory_.GetWeakPtr()));
  }
  cdm_document_service_->IsVerifiedAccessEnabled(base::BindOnce(
      &ChromeOsCdmFactory::OnVerifiedAccessEnabled, weak_factory_.GetWeakPtr(),
      cdm_config, session_message_cb, session_closed_cb, session_keys_change_cb,
      session_expiration_update_cb, std::move(cdm_created_cb)));
}

// static
void ChromeOsCdmFactory::GetHwConfigData(
    ChromeOsCdmContext::GetHwConfigDataCB callback) {
  if (!GetFactoryTaskRunner()->RunsTasksInCurrentSequence()) {
    GetFactoryTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ChromeOsCdmFactory::GetHwConfigData,
                                  std::move(callback)));
    return;
  }
  GetBrowserCdmFactoryRemote()->GetHwConfigData(std::move(callback));
}

// static
void ChromeOsCdmFactory::GetScreenResolutions(
    ChromeOsCdmContext::GetScreenResolutionsCB callback) {
  if (!GetFactoryTaskRunner()->RunsTasksInCurrentSequence()) {
    GetFactoryTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ChromeOsCdmFactory::GetScreenResolutions,
                                  std::move(callback)));
    return;
  }
  GetBrowserCdmFactoryRemote()->GetScreenResolutions(std::move(callback));
}

// static
void ChromeOsCdmFactory::AllocateSecureBuffer(
    uint32_t size,
    ChromeOsCdmContext::AllocateSecureBufferCB callback) {
  if (!GetFactoryTaskRunner()->RunsTasksInCurrentSequence()) {
    GetFactoryTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ChromeOsCdmFactory::AllocateSecureBuffer,
                                  size, std::move(callback)));
    return;
  }
  GetBrowserCdmFactoryRemote()->AllocateSecureBuffer(size, std::move(callback));
}

// static
void ChromeOsCdmFactory::ParseEncryptedSliceHeader(
    uint64_t secure_handle,
    uint32_t offset,
    const std::vector<uint8_t>& stream_data,
    ChromeOsCdmContext::ParseEncryptedSliceHeaderCB callback) {
  if (!GetFactoryTaskRunner()->RunsTasksInCurrentSequence()) {
    GetFactoryTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ChromeOsCdmFactory::ParseEncryptedSliceHeader,
                       secure_handle, offset, stream_data,
                       std::move(callback)));
    return;
  }
  GetBrowserCdmFactoryRemote()->ParseEncryptedSliceHeader(
      secure_handle, offset, stream_data, std::move(callback));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// static
void ChromeOsCdmFactory::SetBrowserCdmFactoryRemote(
    mojo::Remote<cdm::mojom::BrowserCdmFactory> remote) {
  GetBrowserCdmFactoryRemoteHolder().ReplaceBrowserCdmFactoryRemote(
      std::move(remote));
  GetFactoryTaskRunner() = base::SequencedTaskRunner::GetCurrentDefault();
}

// static
media::CdmContext* ChromeOsCdmFactory::GetArcCdmContext() {
  static base::NoDestructor<ArcCdmContext> arc_cdm_context;
  return arc_cdm_context.get();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void ChromeOsCdmFactory::OnVerifiedAccessEnabled(
    const media::CdmConfig& cdm_config,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionKeysChangeCB& session_keys_change_cb,
    const media::SessionExpirationUpdateCB& session_expiration_update_cb,
    media::CdmCreatedCB cdm_created_cb,
    bool enabled) {
  if (!enabled) {
    DVLOG(1)
        << "Not using Chrome OS CDM factory due to Verified Access disabled";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(cdm_created_cb), nullptr,
                       media::CreateCdmStatus::kCrOsVerifiedAccessDisabled));
    return;
  }
  // If we haven't retrieved the remote CDM factory, do that first.
  if (!remote_factory_) {
    // Now invoke the call to create the Mojo interface for the CDM factory. We
    // need to invoke the CreateFactory call on the factory task runner, but
    // we then need to process the callback on the current runner, so there's a
    // few layers of indirection here.
    GetFactoryTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CreateFactoryOnTaskRunner, cdm_config.key_system,
            base::BindPostTaskToCurrentDefault(base::BindOnce(
                &ChromeOsCdmFactory::OnCreateFactory,
                weak_factory_.GetWeakPtr(), cdm_config, session_message_cb,
                session_closed_cb, session_keys_change_cb,
                session_expiration_update_cb, std::move(cdm_created_cb)))));
    return;
  }

  // Create the remote CDM in the daemon and then pass that into our adapter
  // that converts the media::ContentDecryptionModule/Decryptor calls into
  // chromeos::cdm::mojom::ContentDecryptionModule calls.
  CreateCdm(cdm_config, session_message_cb, session_closed_cb,
            session_keys_change_cb, session_expiration_update_cb,
            std::move(cdm_created_cb));
}

void ChromeOsCdmFactory::OnCreateFactory(
    const media::CdmConfig& cdm_config,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionKeysChangeCB& session_keys_change_cb,
    const media::SessionExpirationUpdateCB& session_expiration_update_cb,
    media::CdmCreatedCB cdm_created_cb,
    mojo::PendingRemote<cdm::mojom::CdmFactory> remote_factory) {
  DVLOG(1) << __func__;
  if (!remote_factory) {
    LOG(ERROR) << "Failed creating the remote CDM factory";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(cdm_created_cb), nullptr,
            media::CreateCdmStatus::kCrOsRemoteFactoryCreationFailed));
    return;
  }
  // Check if this is bound already, which could happen due to asynchronous
  // calls.
  if (!remote_factory_) {
    remote_factory_.Bind(std::move(remote_factory));
    remote_factory_.set_disconnect_handler(
        base::BindOnce(&ChromeOsCdmFactory::OnFactoryMojoConnectionError,
                       weak_factory_.GetWeakPtr()));
  }

  // We have the factory bound, create the CDM.
  CreateCdm(cdm_config, session_message_cb, session_closed_cb,
            session_keys_change_cb, session_expiration_update_cb,
            std::move(cdm_created_cb));
}

void ChromeOsCdmFactory::CreateCdm(
    const media::CdmConfig& /*cdm_config*/,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionKeysChangeCB& session_keys_change_cb,
    const media::SessionExpirationUpdateCB& session_expiration_update_cb,
    media::CdmCreatedCB cdm_created_cb) {
  DVLOG(1) << __func__;
  // Create the storage implementation we are sending to Chrome OS.
  mojo::PendingAssociatedRemote<cdm::mojom::CdmStorage> storage_remote;
  std::unique_ptr<CdmStorageAdapter> storage =
      std::make_unique<CdmStorageAdapter>(
          frame_interfaces_,
          storage_remote.InitWithNewEndpointAndPassReceiver());

  // Create the remote interface for the CDM in Chrome OS.
  mojo::AssociatedRemote<cdm::mojom::ContentDecryptionModule> cros_cdm;
  mojo::PendingAssociatedReceiver<cdm::mojom::ContentDecryptionModule>
      cros_cdm_pending_receiver = cros_cdm.BindNewEndpointAndPassReceiver();

  // Create the adapter that proxies calls between
  // media::ContentDecryptionModule and
  // chromeos::cdm::mojom::ContentDecryptionModule.
  scoped_refptr<ContentDecryptionModuleAdapter> cdm =
      base::WrapRefCounted<ContentDecryptionModuleAdapter>(
          new ContentDecryptionModuleAdapter(
              std::move(storage), std::move(cros_cdm), session_message_cb,
              session_closed_cb, session_keys_change_cb,
              session_expiration_update_cb));

  // Create the OutputProtection interface to pass to the CDM.
  mojo::PendingRemote<cdm::mojom::OutputProtection> output_protection_remote;
  GetFactoryTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetOutputProtectionOnTaskRunner,
          output_protection_remote.InitWithNewPipeAndPassReceiver()));

  url::Origin cdm_origin;
  {
    // TODO (crbug.com/368792274): Refactor the GetCdmOrigin mojo call to not be
    // a sync call.
    base::ScopedAllowBaseSyncPrimitives allow_sync_mojo_call;
    frame_interfaces_->GetCdmOrigin(&cdm_origin);
  }

  // Now create the remote CDM instance that links everything up.
  remote_factory_->CreateCdm(
      cdm->GetClientInterface(), std::move(storage_remote),
      std::move(output_protection_remote), cdm_origin.host(),
      std::move(cros_cdm_pending_receiver),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &OnCdmCreated, std::move(cdm_created_cb), std::move(cdm))));
}

void ChromeOsCdmFactory::OnFactoryMojoConnectionError() {
  DVLOG(1) << __func__;
  remote_factory_.reset();
}

void ChromeOsCdmFactory::OnVerificationMojoConnectionError() {
  DVLOG(1) << __func__;
  cdm_document_service_.reset();
}

}  // namespace chromeos
