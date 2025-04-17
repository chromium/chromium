// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TESTING_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TESTING_UTILS_H_

class TestingPrefServiceSimple;

namespace ash::babelorca {

inline constexpr char kTranslationTargetLocale[] = "de-DE";
inline constexpr char kCaptionsTextSize[] = "20%";
inline constexpr char kCaptionsTextFont[] = "aerial";
inline constexpr char kCaptionsTextColor[] = "255,99,71";
inline constexpr char kCaptionsBackgroundColor[] = "90,255,50";
inline constexpr char kCaptionsTextShadow[] = "10px";
inline constexpr int kCaptionsTextOpacity = 50;
inline constexpr int kCaptionsBackgroundOpacity = 30;

void RegisterPrefsForTesting(TestingPrefServiceSimple* registry);

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TESTING_UTILS_H_
