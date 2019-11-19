// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PLEASE NOTE: this is a copy with modifications from
// /chrome/browser/speech/extension_api
// It is temporary until a refactoring to move the chrome TTS implementation up
// into components and extensions/components can be completed.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_API_TTS_TTS_EXTENSION_API_H_
#define CHROMECAST_BROWSER_EXTENSIONS_API_TTS_TTS_EXTENSION_API_H_

#include <string>

#include "chromecast/browser/tts/tts_controller.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

namespace content {
class BrowserContext;
}

const char* TtsEventTypeToString(TtsEventType event_type);
TtsEventType TtsEventTypeFromString(const std::string& str);

namespace extensions {

class TtsSpeakFunction : public ExtensionFunction {
 private:
  ~TtsSpeakFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tts.speak", TTS_SPEAK)
};

class TtsStopSpeakingFunction : public ExtensionFunction {
 private:
  ~TtsStopSpeakingFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tts.stop", TTS_STOP)
};

class TtsPauseFunction : public ExtensionFunction {
 private:
  ~TtsPauseFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tts.pause", TTS_PAUSE)
};

class TtsResumeFunction : public ExtensionFunction {
 private:
  ~TtsResumeFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tts.resume", TTS_RESUME)
};

class TtsIsSpeakingFunction : public ExtensionFunction {
 private:
  ~TtsIsSpeakingFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tts.isSpeaking", TTS_ISSPEAKING)
};

class TtsGetVoicesFunction : public ExtensionFunction {
 private:
  ~TtsGetVoicesFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tts.getVoices", TTS_GETVOICES)
};

class TtsAPI : public BrowserContextKeyedAPI {
 public:
  explicit TtsAPI(content::BrowserContext* context);
  ~TtsAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<TtsAPI>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<TtsAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "TtsAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
};

}  // namespace extensions

#endif  // CHROMECAST_BROWSER_EXTENSIONS_API_TTS_TTS_EXTENSION_API_H_
