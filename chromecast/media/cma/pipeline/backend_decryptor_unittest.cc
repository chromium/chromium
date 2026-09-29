// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/backend_decryptor.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "chromecast/media/base/cast_decoder_buffer_impl.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {

enum class DecryptorMode {
  kPendingAndAbortOnDestruction,
  kImmediateSuccess,
};

DecryptorMode g_decryptor_mode = DecryptorMode::kPendingAndAbortOnDestruction;

class TestAudioDecryptor;
TestAudioDecryptor* g_last_decryptor = nullptr;

// AudioDecryptor test implementation whose behavior is configurable for tests.
class TestAudioDecryptor : public MediaPipelineBackend::AudioDecryptor {
 public:
  TestAudioDecryptor() { g_last_decryptor = this; }

  ~TestAudioDecryptor() override {
    if (g_last_decryptor == this) {
      g_last_decryptor = nullptr;
    }
    if (g_decryptor_mode == DecryptorMode::kPendingAndAbortOnDestruction &&
        has_pending_ && delegate_) {
      delegate_->OnPushBufferForDecryptComplete(
          MediaPipelineBackend::kBufferFailed);
    }
  }

  void SetDelegate(Delegate* delegate) override { delegate_ = delegate; }

  BufferStatus PushBufferForDecrypt(CastDecoderBuffer* buffer,
                                    uint8_t* output) override {
    if (g_decryptor_mode == DecryptorMode::kPendingAndAbortOnDestruction) {
      has_pending_ = true;
      return MediaPipelineBackend::kBufferPending;
    }

    if (g_decryptor_mode == DecryptorMode::kImmediateSuccess) {
      if (delegate_) {
        delegate_->OnDecryptComplete(true);
      }
      return MediaPipelineBackend::kBufferSuccess;
    }

    return MediaPipelineBackend::kBufferSuccess;
  }

  Delegate* delegate() const { return delegate_; }

 private:
  Delegate* delegate_ = nullptr;
  bool has_pending_ = false;
};

void OnDecrypted(bool* called,
                 bool* out_success,
                 size_t* out_buffer_count,
                 bool success,
                 StreamDecryptor::BufferQueue buffers) {
  if (called) {
    *called = true;
  }
  if (out_success) {
    *out_success = success;
  }
  if (out_buffer_count) {
    *out_buffer_count = buffers.size();
  }
}

}  // namespace

// Provide the factory function so BackendDecryptor creates TestAudioDecryptor.
MediaPipelineBackend::AudioDecryptor*
MediaPipelineBackend::CreateAudioDecryptor(EncryptionScheme scheme,
                                           TaskRunner* task_runner) {
  return new TestAudioDecryptor();
}

class BackendDecryptorTest : public testing::Test {
 protected:
  void SetUp() override {
    g_decryptor_mode = DecryptorMode::kPendingAndAbortOnDestruction;
    g_last_decryptor = nullptr;
  }

  void TearDown() override { g_last_decryptor = nullptr; }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(BackendDecryptorTest, DelegateNotificationOnDestruction) {
  auto decryptor =
      std::make_unique<BackendDecryptor>(EncryptionScheme::kAesCbc);

  bool called = false;
  bool success = true;
  decryptor->Init(
      base::BindRepeating(&OnDecrypted, &called, &success, nullptr));

  // Push a buffer; decryptor returns kBufferPending so completion is deferred.
  decryptor->Decrypt(base::MakeRefCounted<CastDecoderBufferImpl>(16));

  // Reset decryptor. During destruction, AudioDecryptor notifies delegate.
  decryptor.reset();

  EXPECT_TRUE(called);
  EXPECT_FALSE(success);
}

TEST_F(BackendDecryptorTest, DestructionWithoutInit) {
  auto decryptor =
      std::make_unique<BackendDecryptor>(EncryptionScheme::kAesCbc);

  // Push buffer without calling Init().
  decryptor->Decrypt(base::MakeRefCounted<CastDecoderBufferImpl>(16));

  // Reset decryptor; should complete cleanly without initialized callback.
  decryptor.reset();
}

TEST_F(BackendDecryptorTest, DestructionWithoutPendingOperation) {
  auto decryptor =
      std::make_unique<BackendDecryptor>(EncryptionScheme::kAesCbc);

  bool called = false;
  decryptor->Init(base::BindRepeating(&OnDecrypted, &called, nullptr, nullptr));

  // Reset decryptor without pushing any buffers.
  decryptor.reset();

  EXPECT_FALSE(called);
}

TEST_F(BackendDecryptorTest, ImmediateSuccessfulDecryption) {
  g_decryptor_mode = DecryptorMode::kImmediateSuccess;

  auto decryptor =
      std::make_unique<BackendDecryptor>(EncryptionScheme::kAesCbc);

  bool called = false;
  bool success = false;
  size_t buffer_count = 0;
  decryptor->Init(
      base::BindRepeating(&OnDecrypted, &called, &success, &buffer_count));

  // Push a buffer; decryptor completes immediately.
  decryptor->Decrypt(base::MakeRefCounted<CastDecoderBufferImpl>(16));

  EXPECT_TRUE(called);
  EXPECT_TRUE(success);
  EXPECT_EQ(buffer_count, 1u);
}

TEST_F(BackendDecryptorTest, EmptyPendingBuffersOnDecryptComplete) {
  auto decryptor =
      std::make_unique<BackendDecryptor>(EncryptionScheme::kAesCbc);

  bool called = false;
  decryptor->Init(base::BindRepeating(&OnDecrypted, &called, nullptr, nullptr));

  // Invoke OnDecryptComplete directly when no buffer is pending.
  EXPECT_TRUE(g_last_decryptor);
  if (g_last_decryptor && g_last_decryptor->delegate()) {
    g_last_decryptor->delegate()->OnDecryptComplete(false);
  }

  EXPECT_FALSE(called);
}

TEST_F(BackendDecryptorTest, EndOfStreamDecryption) {
  g_decryptor_mode = DecryptorMode::kImmediateSuccess;

  auto decryptor =
      std::make_unique<BackendDecryptor>(EncryptionScheme::kAesCbc);

  bool called = false;
  bool success = false;
  size_t buffer_count = 0;
  decryptor->Init(
      base::BindRepeating(&OnDecrypted, &called, &success, &buffer_count));

  // Push an EOS buffer; decryptor completes and flushes all ready buffers.
  decryptor->Decrypt(CastDecoderBufferImpl::CreateEOSBuffer());

  EXPECT_TRUE(called);
  EXPECT_TRUE(success);
  EXPECT_EQ(buffer_count, 1u);
}

TEST_F(BackendDecryptorTest, DestructionWithEosPending) {
  auto decryptor =
      std::make_unique<BackendDecryptor>(EncryptionScheme::kAesCbc);

  bool called = false;
  decryptor->Init(base::BindRepeating(&OnDecrypted, &called, nullptr, nullptr));

  // Push an EOS buffer with pending completion.
  decryptor->Decrypt(CastDecoderBufferImpl::CreateEOSBuffer());

  // Reset decryptor while waiting for EOS.
  decryptor.reset();

  EXPECT_FALSE(called);
}

}  // namespace media
}  // namespace chromecast
