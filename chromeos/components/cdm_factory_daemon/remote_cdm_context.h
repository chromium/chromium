// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_REMOTE_CDM_CONTEXT_H_
#define CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_REMOTE_CDM_CONTEXT_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_context.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_context.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

struct RemoteCdmContextTraits;

// Provides the implementation that runs in out of process video decoding that
// proxies the media::CdmContext calls back through a mojom::StableCdmContext
// IPC connection.
class COMPONENT_EXPORT(CDM_FACTORY_DAEMON) RemoteCdmContext
    : public media::CdmContext,
      public ChromeOsCdmContext,
      public media::stable::mojom::CdmContextEventCallback,
      public base::RefCountedThreadSafe<RemoteCdmContext,
                                        RemoteCdmContextTraits> {
 public:
  explicit RemoteCdmContext(
      mojo::PendingRemote<media::stable::mojom::StableCdmContext>
          stable_cdm_context);

  RemoteCdmContext(const RemoteCdmContext&) = delete;
  RemoteCdmContext& operator=(const RemoteCdmContext&) = delete;

  // media::CdmContext:
  std::unique_ptr<media::CallbackRegistration> RegisterEventCB(
      EventCB event_cb) override;
  ChromeOsCdmContext* GetChromeOsCdmContext() override;

  // chromeos::ChromeOsCdmContext:
  void GetHwKeyData(const media::DecryptConfig* decrypt_config,
                    const std::vector<uint8_t>& hw_identifier,
                    GetHwKeyDataCB callback) override;
  void GetHwConfigData(GetHwConfigDataCB callback) override;
  void GetScreenResolutions(GetScreenResolutionsCB callback) override;
  std::unique_ptr<media::CdmContextRef> GetCdmContextRef() override;
  bool UsingArcCdm() const override;
  bool IsRemoteCdm() const override;

  // media::stable::mojom::CdmContextEventCallback:
  void EventCallback(media::CdmContext::Event event) override;

  // Deletes |this| on the correct thread.
  void DeleteOnCorrectThread() const;

 private:
  // For DeleteSoon() in DeleteOnCorrectThread().
  friend class base::DeleteHelper<RemoteCdmContext>;

  ~RemoteCdmContext() override;

  void RegisterForRemoteCallbacks();

  void GetHwKeyDataInternal(
      std::unique_ptr<media::DecryptConfig> decrypt_config,
      const std::vector<uint8_t>& hw_identifier,
      GetHwKeyDataCB callback);

  mojo::Remote<media::stable::mojom::StableCdmContext> stable_cdm_context_;

  scoped_refptr<base::SequencedTaskRunner> mojo_task_runner_;

  mojo::Receiver<media::stable::mojom::CdmContextEventCallback>
      event_callback_receiver_{this};

  media::CallbackRegistry<EventCB::RunType> event_callbacks_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<RemoteCdmContext> weak_ptr_factory_{this};
};

struct COMPONENT_EXPORT(CDM_FACTORY_DAEMON) RemoteCdmContextTraits {
  // Destroys |remote_cdm_context| on the correct thread.
  static void Destruct(const RemoteCdmContext* remote_cdm_context);
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_REMOTE_CDM_CONTEXT_H_
