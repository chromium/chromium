// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_SPEECH_TTS_ENGINE_MANIFEST_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_SPEECH_TTS_ENGINE_MANIFEST_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "base/values.h"
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

// TODO(dtseng): Rename this to TtsEngine, as it encapsulates all data regarding
// an engine, not just its voices.
struct TtsVoices : public Extension::ManifestData {
  TtsVoices();
  ~TtsVoices() override;
  static bool Parse(const base::Value::List& tts_voices,
                    TtsVoices* out_voices,
                    std::u16string* error,
                    Extension* extension);

  std::vector<extensions::TtsVoice> voices;

  // The sample rate at which this engine encodes its audio data.
  std::optional<int> sample_rate;

  // The number of samples in one audio buffer.
  std::optional<int> buffer_size;

  static const std::vector<TtsVoice>* GetTtsVoices(const Extension* extension);
  static const TtsVoices* GetTtsEngineInfo(const Extension* extension);
};

// Parses the "tts_engine" manifest key.
class TtsEngineManifestHandler : public ManifestHandler {
 public:
  TtsEngineManifestHandler();

  TtsEngineManifestHandler(const TtsEngineManifestHandler&) = delete;
  TtsEngineManifestHandler& operator=(const TtsEngineManifestHandler&) = delete;

  ~TtsEngineManifestHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_SPEECH_TTS_ENGINE_MANIFEST_HANDLER_H_
