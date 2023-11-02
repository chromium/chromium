// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SPEECH_RECOGNIZER_H_
#define CHROME_BROWSER_VR_SPEECH_RECOGNIZER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/vr/vr_base_export.h"

namespace content {
class SpeechRecognitionManager;
}

namespace network {
class PendingSharedURLLoaderFactory;
}

namespace vr {

// Note that speech recognition is activated on VR UI thread. This means it
// usually involves 3 threads. In the simplest case, the thread communication
// looks like the following:
//     VR UI thread        Browser thread         IO thread
//          |                    |                    |
//          |----ActivateVS----->|                    |
//          |                    |------Start------>  |
//          |                    |                    |
//          |                    |<-NotifyStateChange-|
//          |<--OnSRStateChanged-|                    |
//          |                    |                    |
//          |                    |<--OnSpeechResult---|
//          |<--OnSRStateChanged-|                    |
//          |                 navigate                |
//          |                    |                    |
// VS = voice search, SR = speech recognition

enum SpeechRecognitionState {
  SPEECH_RECOGNITION_OFF = 0,
  SPEECH_RECOGNITION_READY,
  SPEECH_RECOGNITION_END,
  SPEECH_RECOGNITION_RECOGNIZING,
  SPEECH_RECOGNITION_IN_SPEECH,
  SPEECH_RECOGNITION_TRY_AGAIN,
  SPEECH_RECOGNITION_NETWORK_ERROR,
};

class VoiceResultDelegate {
 public:
  virtual ~VoiceResultDelegate() {}
  virtual void OnVoiceResults(const std::u16string& result) = 0;
};

class BrowserUiInterface;
class SpeechRecognizerOnIO;

// An interface for IO to communicate with browser UI thread.
// This is used by SpeechRecognizerOnIO class who lives on IO thread.
class IOBrowserUIInterface {
 public:
  // Receive a speech recognition result. |is_final| indicated whether the
  // result is an intermediate or final result. If |is_final| is true, then the
  // recognizer stops and no more results will be returned.
  virtual void OnSpeechResult(const std::u16string& query, bool is_final) = 0;

  // Invoked regularly to indicate the average sound volume.
  virtual void OnSpeechSoundLevelChanged(float level) = 0;

  // Invoked when the state of speech recognition is changed.
  virtual void OnSpeechRecognitionStateChanged(
      SpeechRecognitionState new_state) = 0;

 protected:
  virtual ~IOBrowserUIInterface() {}
};

// SpeechRecognizer is a wrapper around the speech recognition engine that
// simplifies its use from the UI thread. This class handles all setup/shutdown,
// collection of results, error cases, and threading.
class VR_BASE_EXPORT SpeechRecognizer : public IOBrowserUIInterface {
 public:
  // |pending_shared_url_loader_factory| must be for a creating a
  // SharedURLLoaderFactory that can be used on the IO Thread.
  SpeechRecognizer(VoiceResultDelegate* delegate,
                   BrowserUiInterface* ui,
                   std::unique_ptr<network::PendingSharedURLLoaderFactory>
                       pending_shared_url_loader_factory,
                   const std::string& accept_language,
                   const std::string& locale);

  SpeechRecognizer(const SpeechRecognizer&) = delete;
  SpeechRecognizer& operator=(const SpeechRecognizer&) = delete;

  ~SpeechRecognizer() override;

  // Start/stop the speech recognizer.
  // Must be called on the UI thread.
  void Start();
  void Stop();

  // Overridden from vr::IOBrowserUIInterface:
  void OnSpeechResult(const std::u16string& query, bool is_final) override;
  void OnSpeechSoundLevelChanged(float level) override;
  void OnSpeechRecognitionStateChanged(
      vr::SpeechRecognitionState new_state) override;

  void GetSpeechAuthParameters(std::string* auth_scope,
                               std::string* auth_token);

  static void SetManagerForTest(content::SpeechRecognitionManager* manager);
  void SetSpeechTimerForTest(std::unique_ptr<base::OneShotTimer> speech_timer);

 private:
  raw_ptr<VoiceResultDelegate> delegate_;
  raw_ptr<BrowserUiInterface> ui_;

  // Non-null until first Start() call, at which point it's moved to the IO
  // thread.
  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_shared_url_loader_factory_;

  const std::string accept_language_;
  std::string locale_;
  std::u16string final_result_;

  // Note that this object is destroyed on IO thread.
  std::unique_ptr<SpeechRecognizerOnIO> speech_recognizer_on_io_;
  base::WeakPtrFactory<SpeechRecognizer> weak_factory_{this};
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SPEECH_RECOGNIZER_H_
