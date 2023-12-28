// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_EFFECTS_TEST_FAKE_AUDIO_SYSTEM_INFO_H_
#define COMPONENTS_MEDIA_EFFECTS_TEST_FAKE_AUDIO_SYSTEM_INFO_H_

#include <string>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/audio/public/cpp/fake_system_info.h"
#include "services/audio/public/mojom/system_info.mojom.h"

namespace media_effects {

class FakeAudioSystemInfo : public audio::FakeSystemInfo {
 public:
  FakeAudioSystemInfo();

  FakeAudioSystemInfo(const FakeAudioSystemInfo&) = delete;
  FakeAudioSystemInfo& operator=(const FakeAudioSystemInfo&) = delete;

  ~FakeAudioSystemInfo() override;

  void Bind(mojo::PendingReceiver<audio::mojom::SystemInfo> receiver);

  // Simulate connecting and disconnecting a mic device with the given
  // `descriptor`.
  void AddFakeInputDevice(const media::AudioDeviceDescription& descriptor);
  void RemoveFakeInputDevice(const std::string& device_id);

  // `callback` will be triggered after the system info replies back to its
  // client in GetInputDeviceDescriptions(). Useful as a stopping point for a
  // base::RunLoop.
  void SetOnRepliedWithInputDeviceDescriptionsCallback(
      base::OnceClosure callback) {
    on_replied_with_input_device_descriptions_ = std::move(callback);
  }

  // `callback` will be triggered when this system info receives a
  // GetInputStreamParameters call.
  void SetOnGetInputStreamParametersCallback(
      base::RepeatingCallback<void(const std::string&)> callback) {
    on_get_input_stream_params_callback_ = std::move(callback);
  }

 protected:
  // audio::mojom::SystemInfo implementation.
  void GetInputStreamParameters(
      const std::string& device_id,
      GetInputStreamParametersCallback callback) override;
  void HasInputDevices(HasInputDevicesCallback callback) override;
  void GetInputDeviceDescriptions(
      GetInputDeviceDescriptionsCallback callback) override;

 private:
  void NotifyDevicesChanged();

  static media::AudioParameters GetAudioParameters();

  mojo::ReceiverSet<audio::mojom::SystemInfo> receivers_;

  base::flat_map</*device_id=*/std::string, media::AudioDeviceDescription>
      input_device_descriptions_;

  base::OnceClosure on_replied_with_input_device_descriptions_ =
      base::DoNothing();
  base::RepeatingCallback<void(const std::string&)>
      on_get_input_stream_params_callback_ = base::DoNothing();
};

}  // namespace media_effects

#endif  // COMPONENTS_MEDIA_EFFECTS_TEST_FAKE_AUDIO_SYSTEM_INFO_H_
