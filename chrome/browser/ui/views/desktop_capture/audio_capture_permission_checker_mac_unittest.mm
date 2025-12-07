// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/audio_capture_permission_checker_mac.h"

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/sync_socket.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "media/mojo/mojom/audio_processing.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace {

class MockStream : public media::mojom::AudioInputStream {
 public:
  MOCK_METHOD0(Record, void());
  MOCK_METHOD1(SetVolume, void(double));
};

class TestStreamFactory : public audio::FakeStreamFactory {
 public:
  TestStreamFactory() : stream_(), stream_receiver_(&stream_) {}
  ~TestStreamFactory() override {
    if (paused_creation_callback_) {
      // Run with failure to avoid a DCHECK when the callback is destroyed.
      std::move(paused_creation_callback_).Run(nullptr, false, std::nullopt);
    }
  }
  void CreateInputStream(
      mojo::PendingReceiver<media::mojom::AudioInputStream> stream_receiver,
      mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
      mojo::PendingRemote<media::mojom::AudioInputStreamObserver>,
      mojo::PendingRemote<media::mojom::AudioLog>,
      const std::string& device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& /*group_id*/,
      uint32_t /*shared_memory_count*/,
      bool /*enable_agc*/,
      media::mojom::AudioProcessingConfigPtr,
      CreateInputStreamCallback callback) override {
    create_input_stream_call_count_++;
    if (pause_creation_) {
      // Save the callback and pause the input stream creation.
      paused_creation_callback_ = std::move(callback);
      // Keep the client alive, otherwise its destruction will cause a pipe
      // close, which will be reported as an error.
      paused_client_.Bind(std::move(client));
      creation_paused_run_loop_.Quit();
      return;
    }
    if (should_fail_) {
      std::move(callback).Run(nullptr, /*initially_muted=*/false, std::nullopt);
      return;
    }

    if (client_) {
      client_.reset();
    }
    // Keep the passed client alive to avoid binding errors.
    client_.Bind(std::move(client));

    if (stream_receiver_.is_bound()) {
      stream_receiver_.reset();
    }
    stream_receiver_.Bind(std::move(stream_receiver));

    base::SyncSocket::CreatePair(&socket1, &socket2);
    std::move(callback).Run({std::in_place,
                             base::UnsafeSharedMemoryRegion::Create(
                                 media::ComputeAudioInputBufferSize(params, 1)),
                             mojo::PlatformHandle(socket1.Take())},
                            /*initially_muted=*/false,
                            base::UnguessableToken::Create());
  }

  void ResumeCreation() {
    // Run the saved callback. Simulate failed creation just because it's
    // simpler.
    EXPECT_TRUE(pause_creation_);
    std::move(paused_creation_callback_).Run(nullptr, false, std::nullopt);
  }

  void WaitForCreationToPause() {
    EXPECT_TRUE(pause_creation_);
    creation_paused_run_loop_.Run();
  }

  void CloseBinding() { stream_receiver_.reset(); }

  // Variables needed for successful input stream creation.
  StrictMock<MockStream> stream_;
  mojo::Remote<media::mojom::AudioInputStreamClient> client_;
  mojo::Receiver<media::mojom::AudioInputStream> stream_receiver_;
  base::SyncSocket socket1, socket2;

  // Variables used to control the input stream creation.
  bool should_fail_ = false;
  bool pause_creation_ = false;
  int create_input_stream_call_count_ = 0;
  base::RunLoop creation_paused_run_loop_;
  CreateInputStreamCallback paused_creation_callback_;
  mojo::Remote<media::mojom::AudioInputStreamClient> paused_client_;
};

class AudioCapturePermissionCheckerMacTest : public testing::Test {
 public:
  AudioCapturePermissionCheckerMacTest() = default;
  ~AudioCapturePermissionCheckerMacTest() override = default;

  void SetUp() override {
    factory_ = std::make_unique<TestStreamFactory>();
    checker_ =
        std::make_unique<AudioCapturePermissionCheckerMac>(base::BindRepeating(
            &AudioCapturePermissionCheckerMacTest::OnPermissionResult,
            base::Unretained(this)));
    checker_->SetAudioStreamFactoryForTest(factory_->MakeRemote());
  }

  void TearDown() override {
    checker_.reset();
    factory_.reset();
  }

  void OnPermissionResult() {
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void SetQuitClosure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<AudioCapturePermissionCheckerMac> checker_;

  std::unique_ptr<TestStreamFactory> factory_;
  base::OnceClosure quit_closure_;
};

TEST_F(AudioCapturePermissionCheckerMacTest, InitialState) {
  EXPECT_EQ(checker_->GetState(),
            AudioCapturePermissionChecker::State::kUnknown);
}

TEST_F(AudioCapturePermissionCheckerMacTest, CheckingState) {
  // Hold the callback so that we can inspect the state before it's been
  // resolved whether we have permission or not.
  factory_->pause_creation_ = true;

  // First step, verify checking step and wait for the stream creation to be
  // paused.
  checker_->RunCheck();
  EXPECT_EQ(checker_->GetState(),
            AudioCapturePermissionChecker::State::kChecking);
  factory_->WaitForCreationToPause();

  // Second step, complete the operation by resuming stream creation and running
  // the run loop.
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());
  factory_->ResumeCreation();
  run_loop.Run();

  EXPECT_NE(checker_->GetState(),
            AudioCapturePermissionChecker::State::kChecking);
}

TEST_F(AudioCapturePermissionCheckerMacTest, PermissionGranted) {
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());
  checker_->RunCheck();
  run_loop.Run();
  EXPECT_EQ(checker_->GetState(),
            AudioCapturePermissionChecker::State::kGranted);
}

TEST_F(AudioCapturePermissionCheckerMacTest, PermissionDenied) {
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());
  factory_->should_fail_ = true;
  checker_->RunCheck();
  run_loop.Run();
  EXPECT_EQ(checker_->GetState(),
            AudioCapturePermissionChecker::State::kDenied);
}

TEST_F(AudioCapturePermissionCheckerMacTest, SecondRunCheckDoesNothing) {
  factory_->pause_creation_ = true;

  // First call should trigger a check.
  checker_->RunCheck();
  factory_->WaitForCreationToPause();
  EXPECT_EQ(factory_->create_input_stream_call_count_, 1);
  EXPECT_EQ(checker_->GetState(),
            AudioCapturePermissionChecker::State::kChecking);

  // Second call should do nothing since a check is in progress.
  checker_->RunCheck();
  // Let the check complete.
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());
  factory_->ResumeCreation();
  run_loop.Run();
  EXPECT_EQ(checker_->GetState(),
            AudioCapturePermissionChecker::State::kDenied);

  // Third call should do nothing since we have a result.
  checker_->RunCheck();
  EXPECT_NE(checker_->GetState(),
            AudioCapturePermissionChecker::State::kChecking);
  EXPECT_EQ(factory_->create_input_stream_call_count_, 1);
}

TEST_F(AudioCapturePermissionCheckerMacTest, DestructionWhileChecking) {
  factory_->pause_creation_ = true;
  checker_->RunCheck();
  EXPECT_EQ(checker_->GetState(),
            AudioCapturePermissionChecker::State::kChecking);
  factory_->WaitForCreationToPause();

  // Destroy the checker. This should not crash.
  checker_.reset();
}

TEST_F(AudioCapturePermissionCheckerMacTest, IpcClosedWhileChecking) {
  factory_->pause_creation_ = true;
  checker_->RunCheck();
  EXPECT_EQ(checker_->GetState(),
            AudioCapturePermissionChecker::State::kChecking);
  factory_->WaitForCreationToPause();

  // Close the IPC by closing the binding. This should cause a connection error
  // on the remote, which should resolve the permission check as kDenied.
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());
  factory_->CloseBinding();
  factory_->ResumeCreation();
  run_loop.Run();

  EXPECT_EQ(checker_->GetState(),
            AudioCapturePermissionChecker::State::kDenied);
}

}  // namespace
