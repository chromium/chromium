// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_STREAM_FACTORY_DELEGATE_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_STREAM_FACTORY_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace audio {
namespace mojom {
class StreamFactory;
}  // namespace mojom
}  // namespace audio

namespace chromeos {
namespace assistant {

// Delegate object to create |audio::mojom::StreamFactory| instances.
class AudioStreamFactoryDelegate {
 public:
  using Callback = base::OnceCallback<void(
      mojo::PendingRemote<audio::mojom::StreamFactory>)>;

  virtual ~AudioStreamFactoryDelegate() = default;

  virtual void RequestAudioStreamFactory(Callback callback) = 0;
};

// A default implementation of the |AudioStreamFactoryDelegate|, that will
// retrieve a stream factory from the |AssistantClient|.
class COMPONENT_EXPORT(ASSISTANT_SERVICE) DefaultAudioStreamFactoryDelegate
    : public AudioStreamFactoryDelegate {
 public:
  DefaultAudioStreamFactoryDelegate() = default;
  DefaultAudioStreamFactoryDelegate(const DefaultAudioStreamFactoryDelegate&) =
      delete;
  DefaultAudioStreamFactoryDelegate& operator=(
      const DefaultAudioStreamFactoryDelegate&) = delete;
  ~DefaultAudioStreamFactoryDelegate() override = default;

  // AudioStreamFactoryDelegate implementation:
  void RequestAudioStreamFactory(Callback callback) override;

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner();
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_STREAM_FACTORY_DELEGATE_H_
