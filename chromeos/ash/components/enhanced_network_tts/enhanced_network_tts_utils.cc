// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/enhanced_network_tts/enhanced_network_tts_utils.h"

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/enhanced_network_tts/enhanced_network_tts_constants.h"
#include "ui/accessibility/ax_text_utils.h"

namespace ash::enhanced_network_tts {
namespace {

// The offsets computed by |ui::GetSentenceEndOffsets| and
// |ui::GetWordEndOffsets| are pointing to the index after the actual end. This
// method converts the offsets to indexes.
void ConvertOffsetsToIndexes(std::vector<int>& vect) {
  for (int& end : vect)
    end -= 1;
}

// The server requires the rate to be between 0.3 and 4.0, in steps of 0.1.
float ClampRateToLimits(float rate) {
  float clampped_rate = std::clamp(rate, kMinRate, kMaxRate);
  // Set the precision to one significant digit.
  return static_cast<float>(static_cast<int>(clampped_rate * 10) / 10.0f);
}

}  // namespace

std::string FormatJsonRequest(const mojom::TtsRequestPtr tts_request) {
  base::Value::Dict request;

  // utterance is sent as {'text': {'text_parts': [<utterance>]} }
  base::Value::List text_parts;
  text_parts.Append(std::move(tts_request->utterance));
  request.SetByDottedPath(kTextPartsPath, std::move(text_parts));

  // Speech rate, Voice and language are sent as
  // {
  //   {'advanced_options':
  //     {
  //       'audio_generation_options': {'speed_factor': <rate>},
  //       'force_language':<lang>
  //     }
  //   },
  //   {'voice_settings':
  //     {'voice_criteria_and_selections':
  //       [{
  //          'selection': {'default_voice':<voice>}},
  //          'criteria': {'language':<lang>}}
  //       }]
  //     }
  //   }
  // }
  // See https://goto.google.com/readaloud-proto for more information.

  // Add speech rate.
  const float rate = ClampRateToLimits(tts_request->rate);
  request.SetByDottedPath(kSpeechFactorPath, base::Value(rate));

  // The voice and language have to be set together to be valid.
  if (tts_request->voice.has_value() && tts_request->lang.has_value()) {
    // Force the server to produce audio based on the current lang.
    request.SetByDottedPath(kForceLanguagePath,
                            base::Value(tts_request->lang.value()));

    // Produce 'voice_criteria_and_selections'.
    base::Value::Dict selection;
    selection.Set(kDefaultVoiceKey,
                  base::Value(std::move(tts_request->voice.value())));
    base::Value::Dict criteria;
    criteria.Set(kLanguageKey, base::Value(tts_request->lang.value()));
    base::Value::Dict voice_selection;
    voice_selection.Set(kSelectionKey, std::move(selection));
    voice_selection.Set(kCriteriaKey, std::move(criteria));
    base::Value::List voice_criteria_and_selections;
    voice_criteria_and_selections.Append(std::move(voice_selection));
    request.SetByDottedPath(kVoiceCriteriaAndSelectionsPath,
                            std::move(voice_criteria_and_selections));
  }

  std::string json_request;
  base::JSONWriter::Write(request, &json_request);
  return json_request;
}

std::vector<uint16_t> FindTextBreaks(const std::u16string& utterance,
                                     const int length_limit) {
  std::vector<uint16_t> breaks;
  DCHECK_GT(length_limit, 0);

  if (utterance.empty())
    return breaks;

  // The input utterance must be pre-trimmed so that it does not start with
  // whitespaces. The ICU break iterator does not work well with text that
  // has whitespaces at start.
  DCHECK(!base::IsUnicodeWhitespace(utterance[0]));

  const int utterance_length = utterance.length();
  if (utterance_length <= length_limit) {
    breaks.push_back(utterance_length - 1);
    return breaks;
  }

  if (length_limit == 1) {
    for (int i = 1; i < utterance_length; i++)
      breaks.push_back(base::checked_cast<uint16_t>(i));
    return breaks;
  }

  std::vector<int> sentence_ends = ui::GetSentenceEndOffsets(utterance);
  ConvertOffsetsToIndexes(sentence_ends);
  std::vector<int> word_ends = ui::GetWordEndOffsets(utterance);
  ConvertOffsetsToIndexes(word_ends);

  const int sentence_ends_length = sentence_ends.size();
  const int word_ends_length = word_ends.size();
  int cur_word_end_index = 0;
  int cur_sentence_end_index = 0;

  int text_start = 0;
  int text_end = -1;

  // Searching for the end of the text piece as long as the |text_end|
  // (i.e., the end of last text piece) is smaller than the last index of the
  // utterance.
  while (text_end < utterance_length - 1) {
    // The start of the current text piece is the end of last piece plus one.
    text_start = text_end + 1;

    // Find the sentence end that is within the |length_limit| distance from the
    // |text_start|.
    while (cur_sentence_end_index < sentence_ends_length &&
           sentence_ends[cur_sentence_end_index] - text_start < length_limit) {
      // Update the |text_end| if we find a sentence end bigger than the prior
      // |text_end|.
      text_end = std::max(text_end, sentence_ends[cur_sentence_end_index]);
      cur_sentence_end_index++;
    }
    // If we have found a sentence end as the end of current text piece,
    // continue to the next search.
    if (text_end >= text_start) {
      breaks.push_back(base::checked_cast<uint16_t>(text_end));
      continue;
    }

    // If there is no qualified sentence end, this means the current sentence
    // is longer than |length_limit|. We keep searching for a word end that is
    // within the |length_limit| distance from the |text_start|.
    while (cur_word_end_index < word_ends_length &&
           word_ends[cur_word_end_index] - text_start < length_limit) {
      // Update the |text_end| if we find a word end bigger than the prior
      // |text_end|.
      text_end = std::max(text_end, word_ends[cur_word_end_index]);
      cur_word_end_index++;
    }
    // If we have found a word end as the end of current text piece, continue to
    // the next search.
    if (text_end >= text_start) {
      breaks.push_back(base::checked_cast<uint16_t>(text_end));
      continue;
    }

    // If there is no sentence end or word end, we just return the index
    // corresponding to the |length_limit| or the end of the utterance. In
    // practice, this means the current word is longer than |length_limit|.
    text_end = std::min(text_start + length_limit - 1, utterance_length - 1);
    breaks.push_back(base::checked_cast<uint16_t>(text_end));
  }

  return breaks;
}

mojom::TtsResponsePtr GetResultOnError(
    const mojom::TtsRequestError error_code) {
  // TODO(crbug.com/40771006): Log errors.
  return mojom::TtsResponse::NewErrorCode(error_code);
}

mojom::TtsResponsePtr UnpackJsonResponse(const base::Value::List& list_data,
                                         const int start_index,
                                         const bool is_last_request) {
  // Depending on the size of input text (n), the list size should be 1 + 2n.
  // The first item in the list is "metadata", then each input text has one
  // dictionary for "text" and another dictionary for "audio". Since we only
  // have one input text (assuming one paragraph only), we should only have a
  // list with a size of three.
  if (list_data.size() != 3) {
    DVLOG(1)
        << "HTTP response for Enhance Network TTS has unexpected JSON data.";
    return GetResultOnError(mojom::TtsRequestError::kReceivedUnexpectedData);
  }

  // Decode timing information. Inside the "text" dictionary, the "timingInfo"
  // is encoded as:
  // "timingInfo":[
  //   {
  //      "text":<string>
  //      "location":{
  //        "textLocation": {"length": <int32>, "offset": <int32>},
  //        "timeLocation": { "timeOffset": <string>, "duration": <string> },
  //        "paragraphTextLocation": {"offset": <int32>, "length": <int32>},
  //   },
  //   ...
  // ]
  std::vector<mojom::TimingInfoPtr> timing_infos;
  const base::Value::Dict& text_dict = list_data[1].GetDict();
  const base::Value::List* timing_info_list =
      text_dict.FindListByDottedPath("text.timingInfo");
  if (timing_info_list == nullptr) {
    DVLOG(1) << "HTTP response for Enhance Network TTS has unexpected timing "
                "info data.";
    return GetResultOnError(mojom::TtsRequestError::kReceivedUnexpectedData);
  }

  for (size_t i = 0; i < timing_info_list->size(); ++i) {
    const base::Value::Dict& timing_info = (*timing_info_list)[i].GetDict();
    const std::string* timing_info_text_ptr = timing_info.FindString("text");
    const std::string* timing_info_timeoffset_ptr =
        timing_info.FindStringByDottedPath("location.timeLocation.timeOffset");
    const std::string* timing_info_duration_ptr =
        timing_info.FindStringByDottedPath("location.timeLocation.duration");
    // If the first item in the timing_info_list does not have a text offset,
    // we default that to 0. If the first item starts with whitespaces, the
    // server will send back the text offset for the item.
    std::optional<int> timing_info_text_offset =
        timing_info.FindIntByDottedPath("location.textLocation.offset");
    if (timing_info_text_offset == std::nullopt && i == 0) {
      timing_info_text_offset = 0;
    }

    if (timing_info_text_offset == std::nullopt || !timing_info_text_ptr ||
        !timing_info_timeoffset_ptr || !timing_info_duration_ptr) {
      continue;
    }
    // The text offset needs to be compensated with the start index of this
    // TtsData.
    timing_infos.push_back(mojom::TimingInfo::New(
        *timing_info_text_ptr, timing_info_text_offset.value() + start_index,
        *timing_info_timeoffset_ptr, *timing_info_duration_ptr));
  }

  // Decode audio data.
  const base::Value::Dict& audio_dict = list_data[2].GetDict();
  const std::string* audio_bytes_ptr =
      audio_dict.FindStringByDottedPath("audio.bytes");
  if (audio_bytes_ptr == nullptr) {
    DVLOG(1) << "HTTP response for Enhance Network TTS has unexpected audio "
                "bytes data.";
    return GetResultOnError(mojom::TtsRequestError::kReceivedUnexpectedData);
  }
  std::string audio_bytes = *audio_bytes_ptr;
  if (!base::Base64Decode(audio_bytes, &audio_bytes)) {
    DVLOG(1) << "Failed to decode the audio data for Enhance Network TTS.";
    return GetResultOnError(mojom::TtsRequestError::kReceivedUnexpectedData);
  }

  std::vector<uint8_t> audio =
      std::vector<uint8_t>(audio_bytes.begin(), audio_bytes.end());
  mojom::TtsDataPtr tts_data = mojom::TtsData::New(
      std::move(audio), std::move(timing_infos), is_last_request);
  // Send the decoded data to the caller.
  return mojom::TtsResponse::NewData(std::move(tts_data));
}

}  // namespace ash::enhanced_network_tts
