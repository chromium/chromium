// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/audio/fake_input_device.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "media/base/audio_block_fifo.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_glitch_info.h"

namespace ash::libassistant {

namespace {

constexpr const char kFakeAudioFile[] = "/tmp/fake_audio.pcm";

std::vector<uint8_t> ReadFileData(base::File* file) {
  const std::vector<uint8_t>::size_type file_size = file->GetLength();
  std::vector<uint8_t> result(file_size);

  bool success = file->ReadAtCurrentPosAndCheck(result);
  DCHECK(success) << "Failed to read input file";
  return result;
}

// Does integer division and rounds the result up.
// Example:
//     1 / 5  --> 1
//     5 / 5  --> 1
//     7 / 5  --> 2
int DivideAndRoundUp(int dividend, int divisor) {
  return (dividend + divisor - 1) / divisor;
}

}  // namespace

// A fake audio input device (also known as a microphone).
// This fake device will wait until the `kFakeAudioFile` exists,
// and it will then forward its data as microphone input.
// Finally it will remove `kFakeAudioFile` (so we do not keep responding the
// same thing over and over again).
class FakeInputDevice {
 public:
  FakeInputDevice() = default;
  ~FakeInputDevice() = default;

  // AudioCapturerSource implementation.
  void Initialize(const media::AudioParameters& params,
                  media::AudioCapturerSource::CaptureCallback* callback) {
    audio_parameters_ = params;
    callback_ = callback;
  }

  void Start() {
    LOG(INFO) << "Starting fake input device";
    PostDelayedTask(FROM_HERE,
                    base::BindOnce(&FakeInputDevice::WaitForAudioFile,
                                   weak_factory_.GetWeakPtr()),
                    base::Milliseconds(100));
  }

  void Stop() {
    LOG(INFO) << "Stopping fake input device";
    callback_ = nullptr;
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

 private:
  void WaitForAudioFile() {
    DCHECK(RunsTasksInCurrentSequence(task_runner_));

    base::FilePath path{kFakeAudioFile};
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                              base::File::FLAG_DELETE_ON_CLOSE);
    if (!file.IsValid()) {
      SendSilence();
      return;
    }

    LOG(INFO) << "Opening audio file " << kFakeAudioFile;
    ReadAudioFile(&file);
    file.Close();
    SendAudio();
  }

  void ReadAudioFile(base::File* file) {
    DCHECK(RunsTasksInCurrentSequence(task_runner_));

    // Some stats about the audio file.
    const media::SampleFormat sample_format = media::kSampleFormatS16;
    const int bytes_per_frame =
        audio_parameters_.GetBytesPerFrame(sample_format);
    const int frame_count = file->GetLength() / bytes_per_frame;
    const int blocks_count =
        DivideAndRoundUp(frame_count, audio_parameters_.frames_per_buffer());

    // Read the file in memory
    std::vector<uint8_t> data = ReadFileData(file);

    // Convert it to a list of blocks of the requested size.
    audio_blocks_ = std::make_unique<media::AudioBlockFifo>(
        audio_parameters_.channels(), audio_parameters_.frames_per_buffer(),
        blocks_count);
    audio_blocks_->Push(data.data(), frame_count, bytes_per_frame);
    // Add silence so the last block is also complete.
    audio_blocks_->PushSilence(audio_blocks_->GetUnfilledFrames());
  }

  void SendAudio() {
    // Send the blocks to the callback
    if (audio_blocks_->available_blocks() <= 0) {
      audio_blocks_.reset();
      SendSilence();
      return;
    }

    const media::AudioBus* block = audio_blocks_->Consume();
    auto delay_in_microseconds =
        audio_parameters_.GetMicrosecondsPerFrame() * block->frames();
    PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeInputDevice::SendAudio, weak_factory_.GetWeakPtr()),
        base::Microseconds(delay_in_microseconds));

    DVLOG(2) << "Send " << block->frames() << " audio frames";
    const base::TimeTicks time = base::TimeTicks::Now();
    if (callback_)
      callback_->Capture(block, time, {}, /*volume=*/0.5,
                         /*key_pressed=*/false);
  }

  // LibAssistant doesn't expect the microphone to stop sending data.
  // Instead, it will check for a long pause to decide the query is finished.
  // This sends this long pause.
  void SendSilence() {
    DCHECK(RunsTasksInCurrentSequence(task_runner_));

    auto audio_packet = media::AudioBus::Create(audio_parameters_);
    auto delay_in_microseconds =
        audio_parameters_.GetMicrosecondsPerFrame() * audio_packet->frames();
    const base::TimeTicks time = base::TimeTicks::Now();
    if (callback_) {
      callback_->Capture(audio_packet.get(), time, {}, /*volume=*/0.5,
                         /*key_pressed=*/false);
    }

    PostDelayedTask(FROM_HERE,
                    base::BindOnce(&FakeInputDevice::WaitForAudioFile,
                                   weak_factory_.GetWeakPtr()),
                    base::Microseconds(delay_in_microseconds));
  }

  void PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) {
    task_runner_->PostDelayedTask(from_here, std::move(task), delay);
  }

  bool RunsTasksInCurrentSequence(
      scoped_refptr<base::SequencedTaskRunner> runner) {
    return runner->RunsTasksInCurrentSequence();
  }

  media::AudioParameters audio_parameters_;
  raw_ptr<media::AudioCapturerSource::CaptureCallback> callback_;
  std::unique_ptr<media::AudioBlockFifo> audio_blocks_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});

  base::WeakPtrFactory<FakeInputDevice> weak_factory_{this};
};

// This wrapper class runs on the caller sequence, `FakeInputDevice` runs on a
// separate background sequence. This wrapper manages the life cycle of
// `FakeInputDevice` and makes sure it's deleted on the right sequence.
class FakeInputDeviceWrapper : public media::AudioCapturerSource {
 public:
  FakeInputDeviceWrapper()
      : fake_input_device_(std::make_unique<FakeInputDevice>()) {}

  // AudioCapturerSource implementation.
  void Initialize(const media::AudioParameters& params,
                  CaptureCallback* callback) override {
    fake_input_device_->Initialize(params, callback);
  }

  void Start() override { fake_input_device_->Start(); }

  void Stop() override { fake_input_device_->Stop(); }

  void SetVolume(double volume) override {}
  void SetAutomaticGainControl(bool enabled) override {}
  void SetOutputDeviceForAec(const std::string& output_device_id) override {}

 private:
  ~FakeInputDeviceWrapper() override {
    fake_input_device_->task_runner()->DeleteSoon(
        FROM_HERE, std::move(fake_input_device_));
  }

  std::unique_ptr<FakeInputDevice> fake_input_device_;
};

scoped_refptr<media::AudioCapturerSource> CreateFakeInputDevice() {
  return base::MakeRefCounted<FakeInputDeviceWrapper>();
}

}  // namespace ash::libassistant
