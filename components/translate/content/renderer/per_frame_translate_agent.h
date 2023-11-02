// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_RENDERER_PER_FRAME_TRANSLATE_AGENT_H_
#define COMPONENTS_TRANSLATE_CONTENT_RENDERER_PER_FRAME_TRANSLATE_AGENT_H_

#include "base/gtest_prod_util.h"
#include "components/translate/content/common/translate.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace translate {

// This class deals with page translation. It is a RenderFrame's
// IPC client for translation requests from the browser process.
//
// This class supports translating sub frames as well as the main
// frame. It is intended to replace TranslateAgent (which only
// translates the main frame. Both classes will exist for a transition
// period in order to control an experiment for proving out sub frame
// translation.
class PerFrameTranslateAgent : public content::RenderFrameObserver,
                               public mojom::TranslateAgent {
 public:
  PerFrameTranslateAgent(content::RenderFrame* render_frame,
                         int world_id,
                         blink::AssociatedInterfaceRegistry* registry);

  PerFrameTranslateAgent(const PerFrameTranslateAgent&) = delete;
  PerFrameTranslateAgent& operator=(const PerFrameTranslateAgent&) = delete;

  ~PerFrameTranslateAgent() override;

  // mojom::TranslateAgent implementation.
  void GetWebLanguageDetectionDetails(
      GetWebLanguageDetectionDetailsCallback callback) override;
  void TranslateFrame(const std::string& translate_script,
                      const std::string& source_lang,
                      const std::string& target_lang,
                      TranslateFrameCallback callback) override;
  void RevertTranslation() override;

 protected:
  // Returns true if the translate library is available, meaning the JavaScript
  // has already been injected in that page.
  virtual bool IsTranslateLibAvailable();

  // Returns true if the translate library has been initialized successfully.
  virtual bool IsTranslateLibReady();

  // Returns true if the translation script has finished translating the page.
  virtual bool HasTranslationFinished();

  // Returns true if the translation script has reported an error performing the
  // translation.
  virtual bool HasTranslationFailed();

  // Returns the error code generated in translate library.
  virtual int64_t GetErrorCode();

  // Starts the translation by calling the translate library.  This method
  // should only be called when the translate script has been injected in the
  // page.  Returns false if the call failed immediately.
  virtual bool StartTranslation();

  // Asks the Translate element in the page what the language of the page is.
  // Can only be called if a translation has happened and was successful.
  // Returns the language code on success, an empty string on failure.
  virtual std::string GetPageSourceLanguage();

  // Adjusts a delay time for a posted task. This is overridden in tests to do
  // tasks immediately by returning 0.
  virtual base::TimeDelta AdjustDelay(int delay_in_milliseconds);

  // Executes the JavaScript code in |script| in the main frame of RenderView.
  virtual void ExecuteScript(const std::string& script);

  // Executes the JavaScript code in |script| in the main frame of RenderView,
  // and returns the boolean returned by the script evaluation if the script was
  // run successfully. Otherwise, returns |fallback| value.
  virtual bool ExecuteScriptAndGetBoolResult(const std::string& script,
                                             bool fallback);

  // Executes the JavaScript code in |script| in the main frame of RenderView,
  // and returns the string returned by the script evaluation if the script was
  // run successfully. Otherwise, returns empty string.
  virtual std::string ExecuteScriptAndGetStringResult(
      const std::string& script);

  // Executes the JavaScript code in |script| in the main frame of RenderView.
  // and returns the number returned by the script evaluation if the script was
  // run successfully. Otherwise, returns 0.0.
  virtual double ExecuteScriptAndGetDoubleResult(const std::string& script);

  // Executes the JavaScript code in |script| in the main frame of RenderView.
  // and returns the integer value returned by the script evaluation if the
  // script was run successfully. Otherwise, returns 0.
  virtual int64_t ExecuteScriptAndGetIntegerResult(const std::string& script);

 private:
  FRIEND_TEST_ALL_PREFIXES(PerFrameTranslateAgentTest,
                           TestBuildTranslationScript);

  // Converts language code to the one used in server supporting list.
  static void ConvertLanguageCodeSynonym(std::string* code);

  // Sets receiver for translate messages from browser process.
  void BindReceiver(
      mojo::PendingAssociatedReceiver<mojom::TranslateAgent> receiver);

  // Builds the translation JS used to translate from source_lang to
  // target_lang.
  static std::string BuildTranslationScript(const std::string& source_lang,
                                            const std::string& target_lang);

  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // Cancels any translation that is currently being performed.  This does not
  // revert existing translations.
  void CancelPendingTranslation();

  // Checks if the current running page translation is finished or errored and
  // notifies the browser accordingly.  If the translation has not terminated,
  // posts a task to check again later. |check_count| is used to limit the
  // number of retries.
  void CheckTranslateStatus(int check_count);

  // Called by TranslateFrame to do the actual translation.  |count| is used to
  // limit the number of retries.
  void TranslateFrameImpl(int count);

  // Sends a message to the browser to notify it that the translation failed
  // with |error|.
  void NotifyBrowserTranslationFailed(TranslateErrors error);

  // The states associated with the current translation.
  TranslateFrameCallback translate_callback_pending_;
  std::string source_lang_;
  std::string target_lang_;

  // The world ID to use for script execution.
  int world_id_;

  mojo::AssociatedReceiver<mojom::TranslateAgent> receiver_{this};

  // Method factory used to make calls to TranslateFrameImpl.
  base::WeakPtrFactory<PerFrameTranslateAgent> weak_method_factory_{this};
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_RENDERER_PER_FRAME_TRANSLATE_AGENT_H_
