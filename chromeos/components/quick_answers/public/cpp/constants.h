// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_CONSTANTS_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_CONSTANTS_H_

namespace quick_answers {

// TODO(b/340628526): Merge `quick_answers::IntentType` and
// `QuickAnswersView::Intent` to this `Intent`. Each is used for slightly
// different purposes.
enum class Intent {
  kDefinition,
  kTranslation,
  kUnitConversion,
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_CONSTANTS_H_
