// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_API_TTS_TTS_EXTENSION_API_CONSTANTS_H_
#define CHROMECAST_BROWSER_EXTENSIONS_API_TTS_TTS_EXTENSION_API_CONSTANTS_H_

namespace tts_extension_api_constants {

extern const char kCharIndexKey[];
extern const char kLengthKey[];
extern const char kDesiredEventTypesKey[];
extern const char kEnqueueKey[];
extern const char kErrorMessageKey[];
extern const char kEventTypeKey[];
extern const char kEventTypesKey[];
extern const char kExtensionIdKey[];
extern const char kGenderKey[];
extern const char kIsFinalEventKey[];
extern const char kLangKey[];
extern const char kOnEventKey[];
extern const char kPitchKey[];
extern const char kRateKey[];
extern const char kRemoteKey[];
extern const char kRequiredEventTypesKey[];
extern const char kSrcIdKey[];
extern const char kVoiceNameKey[];
extern const char kVolumeKey[];

extern const char kEventTypeCancelled[];
extern const char kEventTypeEnd[];
extern const char kEventTypeError[];
extern const char kEventTypeInterrupted[];
extern const char kEventTypeMarker[];
extern const char kEventTypePause[];
extern const char kEventTypeResume[];
extern const char kEventTypeSentence[];
extern const char kEventTypeStart[];
extern const char kEventTypeWord[];

extern const char kErrorExtensionIdMismatch[];
extern const char kErrorInvalidLang[];
extern const char kErrorInvalidPitch[];
extern const char kErrorInvalidRate[];
extern const char kErrorInvalidVolume[];
extern const char kErrorMissingPauseOrResume[];
extern const char kErrorUndeclaredEventType[];
extern const char kErrorUtteranceTooLong[];

}  // namespace tts_extension_api_constants.
#endif  // CHROMECAST_BROWSER_EXTENSIONS_API_TTS_TTS_EXTENSION_API_CONSTANTS_H_
