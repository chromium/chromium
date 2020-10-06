// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_QUICK_ANSWERS_PREFS_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_QUICK_ANSWERS_PREFS_H_

class PrefRegistrySimple;

namespace chromeos {
namespace quick_answers {
namespace prefs {

extern const char kQuickAnswersConsented[];
extern const char kQuickAnswersNoticeImpressionCount[];
extern const char kQuickAnswersNoticeImpressionDuration[];

// Registers Quick Answers specific profile preferences for browser prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace quick_answers
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_QUICK_ANSWERS_PREFS_H_
