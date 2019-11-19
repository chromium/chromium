// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_MEDIA_DATA_SOURCE_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_MEDIA_DATA_SOURCE_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "chromeos/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "libassistant/shared/public/platform_audio_output.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace assistant {

// Class to provide media data source for audio stream decoder.
// Internally it will read media data from |delegate_|.
class AudioMediaDataSource : public mojom::AssistantMediaDataSource {
 public:
  AudioMediaDataSource(
      mojo::PendingReceiver<mojom::AssistantMediaDataSource> receiver,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~AudioMediaDataSource() override;

  // mojom::MediaDataSource implementation.
  // Called by utility process. Must be called after |set_delegate()|.
  // The caller must wait for callback to finish before issuing the next read.
  void Read(uint32_t size,
            mojom::AssistantMediaDataSource::ReadCallback callback) override;

  // Called by AudioStreamHandler on main thread.
  void set_delegate(assistant_client::AudioOutput::Delegate* delegate) {
    delegate_ = delegate;
  }

 private:
  // Called on main thread.
  void OnFillBuffer(mojom::AssistantMediaDataSource::ReadCallback callback,
                    int bytes_filled);

  mojo::Receiver<mojom::AssistantMediaDataSource> receiver_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  assistant_client::AudioOutput::Delegate* delegate_ = nullptr;

  std::vector<uint8_t> source_buffer_;

  base::WeakPtrFactory<AudioMediaDataSource> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioMediaDataSource);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_MEDIA_DATA_SOURCE_H_
