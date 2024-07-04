// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_TEST_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_TEST_UTILS_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/values.h"

namespace ash::enhanced_network_tts {

// The accuracy used to compare two doubles.
inline constexpr double kDoubleCompareAccuracy = 0.000001;

// The template for a full request that contains utterance, rate, voice name,
// and language. See https://goto.google.com/readaloud-proto for more
// information.
inline constexpr char kFullRequestTemplate[] =
    R"({
        "advanced_options": {
          "audio_generation_options": {"speed_factor": %.1f},
          "force_language": "%s"
        },
        "text": {
          "text_parts": ["%s"]
        },
        "voice_settings": {
          "voice_criteria_and_selections": [{
            "criteria": {"language": "%s"},
            "selection": {"default_voice": "%s"}
          }]
        }
      })";

// The template for a simple request that only contains utterance and rate.
// See https://goto.google.com/readaloud-proto for more information.
inline constexpr char kSimpleRequestTemplate[] =
    R"({"advanced_options": {
          "audio_generation_options": {"speed_factor": %.1f}
        },
        "text": {"text_parts": ["%s"]}})";

// Template for a server response.
inline constexpr char kTemplateResponse[] =
    R"([
        {"metadata": {}},
        {"text": {
          "timingInfo": [
            {
              "text": "test1",
              "location": {
                "textLocation": {"length": 5},
                "timeLocation": {
                  "timeOffset": "0.01s",
                  "duration": "0.14s"
                }
              }
            },
            {
              "text": "test2",
              "location": {
                "textLocation": {"length": 5, "offset": 6},
                "timeLocation": {
                  "timeOffset": "0.16s",
                  "duration": "0.17s"
                }
              }
            }
          ]}
        },
        {"audio": {"bytes": "%s"}}
      ])";

// Create a correct request based on the |kFullRequestTemplate|.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ENHANCED_NETWORK_TTS)
std::string CreateCorrectRequest(const std::string& input_text,
                                 float rate,
                                 const std::string& voice_name,
                                 const std::string& lang);

// Create a correct request based on the |kSimpleRequestTemplate|.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ENHANCED_NETWORK_TTS)
std::string CreateCorrectRequest(const std::string& input_text, float rate);

// Create a server response based on the |kTemplateResponse|.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ENHANCED_NETWORK_TTS)
std::string CreateServerResponse(const std::vector<uint8_t>& expected_output);

// Check if two request strings are equal. Use the |kDoubleCompareAccuracy| when
// checking speech rates. This assumes the two strings follow
// |kFullRequestTemplate| or |kSimpleRequestTemplate|, and speech rates have one
// decimal digit only.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ENHANCED_NETWORK_TTS)
bool AreRequestsEqual(const std::string& json_a, const std::string& json_b);

}  // namespace ash::enhanced_network_tts

#endif  // CHROMEOS_ASH_COMPONENTS_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_TEST_UTILS_H_
