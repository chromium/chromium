// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_CONSTANTS_H_

#include <stddef.h>

namespace ash::enhanced_network_tts {

// The max size for a response. Set to 5MB to provide more buffer. The mp3 file
// has 32Kbps bitrate. One minute of audio is about 240KB.
inline constexpr size_t kEnhancedNetworkTtsMaxResponseSize = 5 * 1024 * 1024;

// The max speech rate.
inline constexpr float kMaxRate = 4.0f;

// The min speech rate.
inline constexpr float kMinRate = 0.3f;

// The HTTP request header in which the API key should be transmitted.
inline constexpr char kGoogApiKeyHeader[] = "X-Goog-Api-Key";

// The server URL for the ReadAloud API.
inline constexpr char kReadAloudServerUrl[] =
    "https://readaloud.googleapis.com//v1:generateAudioDocStream";

// Upload type for the network request.
inline constexpr char kNetworkRequestUploadType[] = "application/json";

// Keys and paths in the request.
// See https://goto.google.com/readaloud-proto for more information.
inline constexpr char kDefaultVoiceKey[] = "default_voice";

inline constexpr char kLanguageKey[] = "language";

inline constexpr char kSelectionKey[] = "selection";

inline constexpr char kCriteriaKey[] = "criteria";

inline constexpr char kTextPartsPath[] = "text.text_parts";

inline constexpr char kSpeechFactorPath[] =
    "advanced_options.audio_generation_options.speed_factor";

inline constexpr char kForceLanguagePath[] = "advanced_options.force_language";

inline constexpr char kVoiceCriteriaAndSelectionsPath[] =
    "voice_settings.voice_criteria_and_selections";

}  // namespace ash::enhanced_network_tts

#endif  // CHROMEOS_ASH_COMPONENTS_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_CONSTANTS_H_
