// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/speech_recognizer.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/test/mock_browser_ui_interface.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_error.mojom.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_result.mojom.h"

namespace vr {

static const int kTestSessionId = 1;
const char kTestInterimResult[] = "kitten";
const char kTestResult[] = "cat";
const char kTestResultMultiple[] = "cat video";

enum FakeRecognitionEvent {
  RECOGNITION_START = 0,
  RECOGNITION_END,
  NETWORK_ERROR,
  SOUND_START,
  SOUND_END,
  AUDIO_START,
  AUDIO_END,
  INTERIM_RESULT,
  FINAL_RESULT,
  MULTIPLE_FINAL_RESULT,
};

// A SharedURLLoaderFactory that hangs.
class FakeSharedURLLoaderFactory : public network::SharedURLLoaderFactory {
 public:
  FakeSharedURLLoaderFactory() {}

  // network::mojom::URLLoaderFactory:

  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    test_url_loader_factory_.Clone(std::move(request));
  }

  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    test_url_loader_factory_.CreateLoaderAndStart(
        std::move(loader), routing_id, request_id, options, request,
        std::move(client), traffic_annotation);
  }

  // network::SharedURLLoaderFactory:
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override {
    NOTREACHED();
    return nullptr;
  }

 private:
  friend class base::RefCounted<FakeSharedURLLoaderFactory>;

  ~FakeSharedURLLoaderFactory() override {}

  network::TestURLLoaderFactory test_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeSharedURLLoaderFactory);
};

// Returns a SharedURLLoaderFactory that hangs.
class FakeSharedURLLoaderFactoryInfo
    : public network::SharedURLLoaderFactoryInfo {
 public:
  FakeSharedURLLoaderFactoryInfo() {}
  ~FakeSharedURLLoaderFactoryInfo() override {}

 protected:
  friend class network::SharedURLLoaderFactory;

  // network::SharedURLLoaderFactoryInfo:
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override {
    return base::MakeRefCounted<FakeSharedURLLoaderFactory>();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeSharedURLLoaderFactoryInfo);
};

class FakeSpeechRecognitionManager : public content::SpeechRecognitionManager {
 public:
  FakeSpeechRecognitionManager() {}
  ~FakeSpeechRecognitionManager() override {}

  // SpeechRecognitionManager methods.
  int CreateSession(
      const content::SpeechRecognitionSessionConfig& config) override {
    session_ctx_ = config.initial_context;
    session_config_ = config;
    session_id_ = kTestSessionId;
    return session_id_;
  }

  void StartSession(int session_id) override {}

  void AbortSession(int session_id) override {
    DCHECK(session_id_ == session_id);
    session_id_ = 0;
  }

  void AbortAllSessionsForRenderFrame(int render_process_id,
                                      int render_frame_id) override {}
  void StopAudioCaptureForSession(int session_id) override {}

  const content::SpeechRecognitionSessionConfig& GetSessionConfig(
      int session_id) const override {
    DCHECK(session_id_ == session_id);
    return session_config_;
  }

  content::SpeechRecognitionSessionContext GetSessionContext(
      int session_id) const override {
    DCHECK(session_id_ == session_id);
    return session_ctx_;
  }

  void FakeSpeechRecognitionEvent(FakeRecognitionEvent event) {
    if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
      base::PostTaskWithTraits(
          FROM_HERE, {content::BrowserThread::IO},
          base::BindOnce(
              &FakeSpeechRecognitionManager::FakeSpeechRecognitionEvent,
              base::Unretained(this), event));
      return;
    }
    DCHECK(GetActiveListener());
    blink::mojom::SpeechRecognitionError error(
        blink::mojom::SpeechRecognitionErrorCode::kNetwork,
        blink::mojom::SpeechAudioErrorDetails::kNone);
    switch (event) {
      case RECOGNITION_START:
        GetActiveListener()->OnRecognitionStart(kTestSessionId);
        break;
      case RECOGNITION_END:
        GetActiveListener()->OnRecognitionEnd(kTestSessionId);
        break;
      case NETWORK_ERROR:
        GetActiveListener()->OnRecognitionError(kTestSessionId, error);
        break;
      case SOUND_START:
        GetActiveListener()->OnSoundStart(kTestSessionId);
        break;
      case INTERIM_RESULT:
        SendFakeInterimResults();
        break;
      case FINAL_RESULT:
        SendFakeFinalResults();
        break;
      case MULTIPLE_FINAL_RESULT:
        SendFakeMultipleFinalResults();
        break;
      default:
        NOTREACHED();
    }
  }

  void SendFakeInterimResults() {
    if (!session_id_)
      return;

    SendRecognitionResult(kTestInterimResult, true);
  }

  void SendFakeFinalResults() {
    if (!session_id_)
      return;

    SendRecognitionResult(kTestResult, false);
    FakeSpeechRecognitionEvent(RECOGNITION_END);
    session_id_ = 0;
  }

  void SendFakeMultipleFinalResults() {
    if (!session_id_)
      return;

    SendRecognitionResult(kTestResult, false);
    SendRecognitionResult(kTestResultMultiple, false);
    FakeSpeechRecognitionEvent(RECOGNITION_END);
    session_id_ = 0;
  }

 private:
  void SendRecognitionResult(const char* string, bool is_provisional) {
    content::SpeechRecognitionEventListener* listener = GetActiveListener();
    if (!listener)
      return;
    listener->OnAudioStart(session_id_);
    listener->OnAudioEnd(session_id_);

    blink::mojom::SpeechRecognitionResultPtr result =
        blink::mojom::SpeechRecognitionResult::New();
    result->hypotheses.push_back(blink::mojom::SpeechRecognitionHypothesis::New(
        base::ASCIIToUTF16(string), 1.0));
    result->is_provisional = is_provisional;
    std::vector<blink::mojom::SpeechRecognitionResultPtr> results;
    results.push_back(std::move(result));

    listener->OnRecognitionResults(session_id_, results);
  }

  content::SpeechRecognitionEventListener* GetActiveListener() {
    DCHECK(session_id_ != 0);
    return session_config_.event_listener.get();
  }

  int session_id_ = 0;
  content::SpeechRecognitionSessionContext session_ctx_;
  content::SpeechRecognitionSessionConfig session_config_;

  DISALLOW_COPY_AND_ASSIGN(FakeSpeechRecognitionManager);
};

class MockVoiceSearchDelegate : public VoiceResultDelegate {
 public:
  MockVoiceSearchDelegate() = default;
  ~MockVoiceSearchDelegate() override = default;

  MOCK_METHOD1(OnVoiceResults, void(const base::string16& result));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVoiceSearchDelegate);
};

class SpeechRecognizerTest : public testing::Test {
 public:
  SpeechRecognizerTest()
      : fake_speech_recognition_manager_(new FakeSpeechRecognitionManager()),
        ui_(new MockBrowserUiInterface),
        delegate_(new MockVoiceSearchDelegate),
        speech_recognizer_(new SpeechRecognizer(
            delegate_.get(),
            ui_.get(),
            std::make_unique<FakeSharedURLLoaderFactoryInfo>(),
            "en" /* accept_language */,
            "en" /* locale */)) {
    SpeechRecognizer::SetManagerForTest(fake_speech_recognition_manager_.get());
  }

  ~SpeechRecognizerTest() override {
    SpeechRecognizer::SetManagerForTest(nullptr);
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<FakeSpeechRecognitionManager>
      fake_speech_recognition_manager_;
  std::unique_ptr<MockBrowserUiInterface> ui_;
  std::unique_ptr<MockVoiceSearchDelegate> delegate_;
  std::unique_ptr<SpeechRecognizer> speech_recognizer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SpeechRecognizerTest);
};

TEST_F(SpeechRecognizerTest, ReceivedCorrectSpeechResult) {
  testing::Sequence s;
  EXPECT_CALL(*ui_, SetSpeechRecognitionEnabled(true)).InSequence(s);
  EXPECT_CALL(*ui_, SetRecognitionResult(base::ASCIIToUTF16(kTestResult)))
      .InSequence(s);
  EXPECT_CALL(*delegate_, OnVoiceResults(base::ASCIIToUTF16(kTestResult)))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(*ui_, SetSpeechRecognitionEnabled(false)).InSequence(s);

  speech_recognizer_->Start();
  base::RunLoop().RunUntilIdle();

  // This should not trigger SetRecognitionResult as we don't show interim
  // result.
  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(INTERIM_RESULT);
  base::RunLoop().RunUntilIdle();

  // This should trigger SetRecognitionResult as we received final result.
  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(FINAL_RESULT);
  base::RunLoop().RunUntilIdle();
}

// Test for crbug.com/785051. It is possible that we receive multiple final
// results in one recognition session. We should only navigate once in this
// case.
TEST_F(SpeechRecognizerTest, MultipleResultsTriggerNavigation) {
  testing::Sequence s;
  EXPECT_CALL(*ui_, SetSpeechRecognitionEnabled(true)).InSequence(s);
  EXPECT_CALL(*ui_,
              SetRecognitionResult(base::ASCIIToUTF16(kTestResultMultiple)))
      .InSequence(s);
  EXPECT_CALL(*delegate_,
              OnVoiceResults(base::ASCIIToUTF16(kTestResultMultiple)))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(*ui_, SetSpeechRecognitionEnabled(false)).InSequence(s);

  speech_recognizer_->Start();
  base::RunLoop().RunUntilIdle();

  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(
      MULTIPLE_FINAL_RESULT);
  base::RunLoop().RunUntilIdle();
}

TEST_F(SpeechRecognizerTest, ReceivedSpeechRecognitionStates) {
  speech_recognizer_->Start();
  base::RunLoop().RunUntilIdle();

  testing::Sequence s;
  EXPECT_CALL(*ui_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_RECOGNIZING))
      .InSequence(s);
  EXPECT_CALL(*ui_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_NETWORK_ERROR))
      .InSequence(s);
  EXPECT_CALL(*ui_, OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_END))
      .InSequence(s);

  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(
      RECOGNITION_START);
  base::RunLoop().RunUntilIdle();

  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(NETWORK_ERROR);
  base::RunLoop().RunUntilIdle();

  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(RECOGNITION_END);
  base::RunLoop().RunUntilIdle();
}

TEST_F(SpeechRecognizerTest, NoSoundTimeout) {
  testing::Sequence s;
  EXPECT_CALL(*ui_, SetSpeechRecognitionEnabled(true)).InSequence(s);
  EXPECT_CALL(*ui_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_IN_SPEECH))
      .InSequence(s);
  EXPECT_CALL(*ui_, OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_END))
      .InSequence(s);
  EXPECT_CALL(*ui_, SetSpeechRecognitionEnabled(false)).InSequence(s);

  speech_recognizer_->Start();
  base::RunLoop().RunUntilIdle();

  auto mock_timer = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer* timer_ptr = mock_timer.get();
  speech_recognizer_->SetSpeechTimerForTest(std::move(mock_timer));

  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(SOUND_START);
  base::RunLoop().RunUntilIdle();

  // This should trigger a SPEECH_RECOGNITION_READY state notification.
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();
}

// This test that it is safe to reset speech_recognizer_ on UI thread after post
// a task to start speech recognition on IO thread.
TEST_F(SpeechRecognizerTest, SafeToResetAfterStart) {
  EXPECT_CALL(*ui_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_RECOGNIZING));
  EXPECT_CALL(*ui_, SetRecognitionResult(base::ASCIIToUTF16(kTestResult)))
      .Times(0);

  speech_recognizer_->Start();
  base::RunLoop().RunUntilIdle();

  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(
      RECOGNITION_START);
  base::RunLoop().RunUntilIdle();

  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(FINAL_RESULT);
  // Reset shouldn't crash the test.
  speech_recognizer_.reset(nullptr);
  base::RunLoop().RunUntilIdle();
}

// This test that calling start after stop should still work as expected.
TEST_F(SpeechRecognizerTest, RestartAfterStop) {
  EXPECT_CALL(*ui_, SetRecognitionResult(base::ASCIIToUTF16(kTestResult)))
      .Times(1);

  speech_recognizer_->Start();
  base::RunLoop().RunUntilIdle();

  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(FINAL_RESULT);
  speech_recognizer_->Stop();
  base::RunLoop().RunUntilIdle();

  speech_recognizer_->Start();
  base::RunLoop().RunUntilIdle();

  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(FINAL_RESULT);
  base::RunLoop().RunUntilIdle();
}

}  // namespace vr
