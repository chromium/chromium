// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_AUDIO_AUDIO_MEDIA_DATA_SOURCE_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_AUDIO_AUDIO_MEDIA_DATA_SOURCE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::libassistant {

// Class to provide media data source for audio stream decoder.
// Internally it will read media data from |delegate_|.
class AudioMediaDataSource : public assistant::mojom::AssistantMediaDataSource {
 public:
  explicit AudioMediaDataSource(
      mojo::PendingReceiver<assistant::mojom::AssistantMediaDataSource>
          receiver);

  AudioMediaDataSource(const AudioMediaDataSource&) = delete;
  AudioMediaDataSource& operator=(const AudioMediaDataSource&) = delete;

  ~AudioMediaDataSource() override;

  // assistant::mojom::MediaDataSource implementation.
  // Must be called after |set_delegate()|.
  // The caller must wait for callback to finish before issuing the next read.
  void Read(uint32_t size, ReadCallback callback) override;

  void set_delegate(assistant_client::AudioOutput::Delegate* delegate) {
    delegate_ = delegate;
  }

 private:
  void OnFillBuffer(int bytes_filled);

  mojo::Receiver<AssistantMediaDataSource> receiver_;

  // The callback from |delegate_| runs on a different sequence, so this
  // sequence checker prevents the other methods from being called on the wrong
  // sequence.
  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  raw_ptr<assistant_client::AudioOutput::Delegate> delegate_ = nullptr;

  std::vector<uint8_t> source_buffer_;

  ReadCallback read_callback_;

  base::WeakPtrFactory<AudioMediaDataSource> weak_factory_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_AUDIO_AUDIO_MEDIA_DATA_SOURCE_H_
