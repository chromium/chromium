// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/fake_input_device.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "media/base/audio_block_fifo.h"
#include "media/base/audio_capturer_source.h"

namespace chromeos {
namespace libassistant {

namespace {

constexpr const char kFakeAudioFile[] = "/tmp/fake_audio.pcm";

std::vector<uint8_t> ReadFileData(base::File* file) {
  const int file_size = file->GetLength();
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
// This fake device will wait until the |audio_file| exists,
// and it will then forward its data as microphone input.
// Finally it will remove |audio_file| (so we do not keep responding the
// same thing over and over again).
class FakeInputDevice : public media::AudioCapturerSource {
 public:
  explicit FakeInputDevice(const std::string& audio_file)
      : audio_file_(audio_file) {}

  // AudioCapturerSource implementation.
  void Initialize(const media::AudioParameters& params,
                  CaptureCallback* callback) override {
    audio_parameters_ = params;
    callback_ = callback;
  }

  void Start() override {
    LOG(INFO) << "Starting fake input device";
    PostDelayedTask(FROM_HERE,
                    base::BindOnce(&FakeInputDevice::WaitForAudioFile, this),
                    base::TimeDelta::FromMilliseconds(100));
  }

  void Stop() override {
    LOG(INFO) << "Stopping fake input device";
    task_runner_.reset();
  }
  void SetVolume(double volume) override {}
  void SetAutomaticGainControl(bool enabled) override {}
  void SetOutputDeviceForAec(const std::string& output_device_id) override {}

 private:
  ~FakeInputDevice() override = default;

  void WaitForAudioFile() {
    DCHECK(RunsTasksInCurrentSequence(task_runner_));

    base::FilePath path{audio_file_};
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                              base::File::FLAG_DELETE_ON_CLOSE);
    if (!file.IsValid()) {
      LOG(ERROR)
          << "" << audio_file_
          << " not found. Please run chromeos/assistant/tools/send-audio.sh";

      PostDelayedTask(FROM_HERE,
                      base::BindOnce(&FakeInputDevice::WaitForAudioFile, this),
                      base::TimeDelta::FromSeconds(1));
      return;
    }

    LOG(INFO) << "Opening audio file " << audio_file_;
    ReadAudioFile(&file);
    SendSilence();
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
    media::AudioBlockFifo audio_blocks(audio_parameters_.channels(),
                                       audio_parameters_.frames_per_buffer(),
                                       blocks_count);
    audio_blocks.Push(data.data(), frame_count, bytes_per_frame);
    // Add silence so the last block is also complete.
    audio_blocks.PushSilence(audio_blocks.GetUnfilledFrames());

    // Send the blocks to the callback
    while (audio_blocks.available_blocks()) {
      const media::AudioBus* block = audio_blocks.Consume();
      const base::TimeTicks time = base::TimeTicks::Now();
      callback_->Capture(block, time, /*volume=*/0.5,
                         /*key_pressed=*/false);
    }
  }

  // LibAssistant doesn't expect the microphone to stop sending data.
  // Instead, it will check for a long pause to decide the query is finished.
  // This sends this long pause.
  void SendSilence() {
    DCHECK(RunsTasksInCurrentSequence(task_runner_));

    auto audio_packet = media::AudioBus::Create(audio_parameters_);
    const base::TimeTicks time = base::TimeTicks::Now();
    callback_->Capture(audio_packet.get(), time, /*volume=*/0.5,
                       /*key_pressed=*/false);

    PostDelayedTask(FROM_HERE,
                    base::BindOnce(&FakeInputDevice::SendSilence, this),
                    base::TimeDelta::FromMilliseconds(100));
  }

  void PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) {
    // We use a local copy of the refcounted pointer to the task runner so it
    // can not be deleted between the check and the invocation.
    auto runner = task_runner_;
    if (!runner)
      return;  // This means Stop was called.
    runner->PostDelayedTask(from_here, std::move(task), delay);
  }

  bool RunsTasksInCurrentSequence(
      scoped_refptr<base::SequencedTaskRunner> runner) {
    return (runner == nullptr) || runner->RunsTasksInCurrentSequence();
  }

  std::string audio_file_;
  media::AudioParameters audio_parameters_;
  CaptureCallback* callback_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
};

scoped_refptr<media::AudioCapturerSource> CreateFakeInputDevice() {
  return base::MakeRefCounted<FakeInputDevice>(kFakeAudioFile);
}

}  // namespace libassistant
}  // namespace chromeos
