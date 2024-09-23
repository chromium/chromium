// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/speech/tts_engine_manifest_handler.h"

#include <stddef.h>

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "media/base/limits.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;


TtsVoice::TtsVoice() : remote(false) {}

TtsVoice::TtsVoice(const TtsVoice& other) = default;

TtsVoice::~TtsVoice() = default;

TtsVoices::TtsVoices() = default;
TtsVoices::~TtsVoices() = default;

//  static
bool TtsVoices::Parse(const base::Value::List& tts_voices,
                      TtsVoices* out_voices,
                      std::u16string* error,
                      Extension* extension) {
  for (const base::Value& one_tts_voice_val : tts_voices) {
    if (!one_tts_voice_val.is_dict()) {
      *error = errors::kInvalidTtsVoices;
      return false;
    }

    const base::Value::Dict& one_tts_voice = one_tts_voice_val.GetDict();
    TtsVoice voice_data;
    const base::Value* name = one_tts_voice.Find(keys::kTtsVoicesVoiceName);
    if (name) {
      if (!name->is_string()) {
        *error = errors::kInvalidTtsVoicesVoiceName;
        return false;
      }
      voice_data.voice_name = name->GetString();
    }

    const base::Value* lang = one_tts_voice.Find(keys::kTtsVoicesLang);
    if (lang) {
      if (!lang->is_string() ||
          !l10n_util::IsValidLocaleSyntax(lang->GetString())) {
        *error = errors::kInvalidTtsVoicesLang;
        return false;
      }
      voice_data.lang = lang->GetString();
    }

    const base::Value* remote = one_tts_voice.Find(keys::kTtsVoicesRemote);
    if (remote) {
      if (!remote->is_bool()) {
        *error = errors::kInvalidTtsVoicesRemote;
        return false;
      }
      voice_data.remote = remote->GetBool();
    }

    const base::Value* event_types =
        one_tts_voice.Find(keys::kTtsVoicesEventTypes);
    if (event_types) {
      if (!event_types->is_list()) {
        *error = errors::kInvalidTtsVoicesEventTypes;
        return false;
      }
      for (const base::Value& event_type_val : event_types->GetList()) {
        if (!event_type_val.is_string()) {
          *error = errors::kInvalidTtsVoicesEventTypes;
          return false;
        }
        const std::string& event_type = event_type_val.GetString();
        if (event_type != keys::kTtsVoicesEventTypeEnd &&
            event_type != keys::kTtsVoicesEventTypeError &&
            event_type != keys::kTtsVoicesEventTypeMarker &&
            event_type != keys::kTtsVoicesEventTypeSentence &&
            event_type != keys::kTtsVoicesEventTypeStart &&
            event_type != keys::kTtsVoicesEventTypeWord) {
          *error = errors::kInvalidTtsVoicesEventTypes;
          return false;
        }
        if (voice_data.event_types.find(event_type) !=
            voice_data.event_types.end()) {
          *error = errors::kInvalidTtsVoicesEventTypes;
          return false;
        }
        voice_data.event_types.insert(event_type);
      }
    }
    out_voices->voices.push_back(voice_data);
  }
  return true;
}

// static
const std::vector<TtsVoice>* TtsVoices::GetTtsVoices(
    const Extension* extension) {
  const TtsVoices* engine = TtsVoices::GetTtsEngineInfo(extension);
  return engine ? &engine->voices : nullptr;
}

// static
const TtsVoices* TtsVoices::GetTtsEngineInfo(const Extension* extension) {
  TtsVoices* info =
      static_cast<TtsVoices*>(extension->GetManifestData(keys::kTtsVoices));
  return info;
}

TtsEngineManifestHandler::TtsEngineManifestHandler() = default;

TtsEngineManifestHandler::~TtsEngineManifestHandler() = default;

bool TtsEngineManifestHandler::Parse(Extension* extension,
                                     std::u16string* error) {
  auto info = std::make_unique<TtsVoices>();
  const base::Value::Dict* tts_dict =
      extension->manifest()->available_values().FindDict(keys::kTtsEngine);
  if (!tts_dict) {
    *error = errors::kInvalidTts;
    return false;
  }

  const base::Value* tts_voices = tts_dict->Find(keys::kTtsVoices);
  if (!tts_voices)
    return true;

  if (!tts_voices->is_list()) {
    *error = errors::kInvalidTtsVoices;
    return false;
  }

  if (!TtsVoices::Parse(tts_voices->GetList(), info.get(), error, extension))
    return false;

  const base::Value* tts_engine_sample_rate =
      tts_dict->Find(keys::kTtsEngineSampleRate);
  if (tts_engine_sample_rate) {
    if (!tts_engine_sample_rate->GetIfInt()) {
      *error = errors::kInvalidTtsSampleRateFormat;
      return false;
    }

    info->sample_rate = tts_engine_sample_rate->GetInt();
    if (info->sample_rate < media::limits::kMinSampleRate ||
        info->sample_rate > media::limits::kMaxSampleRate) {
      *error = base::ASCIIToUTF16(base::StringPrintf(
          errors::kInvalidTtsSampleRateRange, media::limits::kMinSampleRate,
          media::limits::kMaxSampleRate));
      return false;
    }
  }

  const base::Value* tts_engine_buffer_size =
      tts_dict->Find(keys::kTtsEngineBufferSize);
  if (tts_engine_buffer_size) {
    if (!tts_engine_buffer_size->GetIfInt()) {
      *error = errors::kInvalidTtsBufferSizeFormat;
      return false;
    }

    // The limits of the buffer size should match those of those found in
    // AudioParameters::IsValid (as should the sample rate limits above).
    constexpr int kMinBufferSize = 1;
    info->buffer_size = tts_engine_buffer_size->GetInt();
    if (info->buffer_size < kMinBufferSize ||
        info->buffer_size > media::limits::kMaxSamplesPerPacket) {
      *error = base::ASCIIToUTF16(
          base::StringPrintf(errors::kInvalidTtsBufferSizeRange, kMinBufferSize,
                             media::limits::kMaxSamplesPerPacket));
      return false;
    }
  }

  if ((!tts_engine_sample_rate && tts_engine_buffer_size) ||
      (tts_engine_sample_rate && !tts_engine_buffer_size)) {
    *error = errors::kInvalidTtsRequiresSampleRateAndBufferSize;
    return false;
  }

  extension->SetManifestData(keys::kTtsVoices, std::move(info));
  return true;
}

base::span<const char* const> TtsEngineManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kTtsEngine};
  return kKeys;
}

}  // namespace extensions
