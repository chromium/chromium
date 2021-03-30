// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_SPEECH_TTS_ENGINE_MANIFEST_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_SPEECH_TTS_ENGINE_MANIFEST_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct TtsVoice {
  TtsVoice();
  TtsVoice(const TtsVoice& other);
  ~TtsVoice();

  std::string voice_name;
  std::string lang;
  std::string gender;
  bool remote;
  std::set<std::string> event_types;
};

struct TtsVoices : public Extension::ManifestData {
  TtsVoices();
  ~TtsVoices() override;
  static bool Parse(const base::ListValue* tts_voices,
                    TtsVoices* out_voices,
                    std::u16string* error,
                    Extension* extension);

  std::vector<extensions::TtsVoice> voices;

  static const std::vector<TtsVoice>* GetTtsVoices(const Extension* extension);
};

// Parses the "tts_engine" manifest key.
class TtsEngineManifestHandler : public ManifestHandler {
 public:
  TtsEngineManifestHandler();
  ~TtsEngineManifestHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(TtsEngineManifestHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_SPEECH_TTS_ENGINE_MANIFEST_HANDLER_H_
