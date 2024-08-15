// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_CONSTANTS_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_CONSTANTS_H_

namespace quick_answers {

// TODO(b/340628526): Merge `quick_answers::IntentType` to this `Intent`. Each
// is used for slightly different purposes.
enum class Intent {
  kDefinition,
  kTranslation,
  kUnitConversion,
};

// An enum used to switch design of Quick Answers UI.
enum class Design {
  // Currently active UI.
  kCurrent,
  // Refreshed Quick Answers UI.
  kRefresh,
  // Design used if Quick Answers is shown as part of Magic Boost.
  kMagicBoost,
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_CONSTANTS_H_
