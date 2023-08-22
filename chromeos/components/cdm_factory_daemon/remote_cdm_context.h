// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_REMOTE_CDM_CONTEXT_H_
#define CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_REMOTE_CDM_CONTEXT_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_context.h"
#include "media/base/cdm_context.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {

// Provides the implementation that runs in out of process video decoding that
// proxies the media::CdmContext calls back through a mojom::StableCdmContext
// IPC connection.
//
// This particular media::CdmContext/chromeos::ChromeOsCdmContext implementation
// has some threading restrictions: after construction, all methods should be
// called on the same sequence unless otherwise noted (in particular, it's safe
// to release the last reference to a RemoteCdmContext on any sequence because
// we guarantee that sequence-affine state is destroyed on the correct
// sequence).
class COMPONENT_EXPORT(CDM_FACTORY_DAEMON) RemoteCdmContext
    : public media::CdmContext,
      public ChromeOsCdmContext,
      public base::RefCountedThreadSafe<RemoteCdmContext> {
 public:
  explicit RemoteCdmContext(
      mojo::PendingRemote<media::stable::mojom::StableCdmContext>
          stable_cdm_context);

  RemoteCdmContext(const RemoteCdmContext&) = delete;
  RemoteCdmContext& operator=(const RemoteCdmContext&) = delete;

  // media::CdmContext:
  std::unique_ptr<media::CallbackRegistration> RegisterEventCB(
      EventCB event_cb) override;
  // GetChromeOsCdmContext() may be called on any sequence.
  ChromeOsCdmContext* GetChromeOsCdmContext() override;

  // chromeos::ChromeOsCdmContext:
  void GetHwKeyData(const media::DecryptConfig* decrypt_config,
                    const std::vector<uint8_t>& hw_identifier,
                    GetHwKeyDataCB callback) override;
  void GetHwConfigData(GetHwConfigDataCB callback) override;
  void GetScreenResolutions(GetScreenResolutionsCB callback) override;
  // GetCdmContextRef() may be called on any sequence.
  std::unique_ptr<media::CdmContextRef> GetCdmContextRef() override;
  // UsingArcCdm() may be called on any sequence.
  bool UsingArcCdm() const override;
  // IsRemoteCdm() may be called on any sequence.
  bool IsRemoteCdm() const override;
  void AllocateSecureBuffer(uint32_t size,
                            AllocateSecureBufferCB callback) override;

 private:
  friend class base::RefCountedThreadSafe<RemoteCdmContext>;

  // MojoSequenceState encapsulates sequence-affine state that is initialized
  // lazily. The custom deleter for |mojo_sequence_state_| guarantees that this
  // state is destroyed on the correct sequence.
  class MojoSequenceState;

  ~RemoteCdmContext() override;

  std::unique_ptr<MojoSequenceState, void (*)(MojoSequenceState*)>
      mojo_sequence_state_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_REMOTE_CDM_CONTEXT_H_
