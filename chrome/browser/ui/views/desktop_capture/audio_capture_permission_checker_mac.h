// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_AUDIO_CAPTURE_PERMISSION_CHECKER_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_AUDIO_CAPTURE_PERMISSION_CHECKER_MAC_H_

#include "base/functional/callback.h"
#include "chrome/browser/ui/views/desktop_capture/audio_capture_permission_checker.h"
#include "media/audio/audio_input_ipc.h"
#include "media/mojo/mojom/audio_stream_factory.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace audio {
class InputIPC;
}  // namespace audio

// AudioCapturePermissionCheckerMac is a helper class to check if the user has
// granted audio capture permission. It is used to determine if a message should
// be shown to the user about how to grant permission. Audio capture permissions
// are verified by checking if creating an audio stream succeeds. All methods
// must be called on the UI thread.
class AudioCapturePermissionCheckerMac : public AudioCapturePermissionChecker,
                                         public media::AudioInputIPCDelegate {
 public:
  using AudioStreamFactoryBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory>)>;

  // Create an AudioCapturePermissionCheckerMac if it is enabled.
  static std::unique_ptr<AudioCapturePermissionCheckerMac> MaybeCreate(
      base::RepeatingCallback<void(void)> callback);

  explicit AudioCapturePermissionCheckerMac(
      base::RepeatingCallback<void(void)> callback);
  ~AudioCapturePermissionCheckerMac() override;

  void SetAudioStreamFactoryForTest(
      mojo::PendingRemote<media::mojom::AudioStreamFactory>
          audio_stream_factory);

  // AudioCapturePermissionChecker implementation.
  State GetState() const override;
  void RunCheck() override;

 private:
  // media::AudioInputIPCDelegate implementation.
  void OnStreamCreated(base::UnsafeSharedMemoryRegion shared_memory_region,
                       base::SyncSocket::ScopedHandle socket_handle,
                       bool initially_muted) override;
  void OnError(media::AudioCapturerSource::ErrorCode code) override;
  void OnMuted(bool is_muted) override;
  void OnIPCClosed() override;

  void OnPermissionUpdate(bool has_permission);

  State audio_permission_state_ = State::kUnknown;
  base::RepeatingCallback<void(void)> callback_;
  std::unique_ptr<audio::InputIPC> input_ipc_;

  mojo::PendingRemote<media::mojom::AudioStreamFactory>
      audio_stream_factory_for_test_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_AUDIO_CAPTURE_PERMISSION_CHECKER_MAC_H_
