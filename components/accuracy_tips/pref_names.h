// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCURACY_TIPS_PREF_NAMES_H_
#define COMPONENTS_ACCURACY_TIPS_PREF_NAMES_H_

namespace accuracy_tips {
namespace prefs {

// TODO(crbug.com/1233024): Delete data for "DisabledUi" preferences after
// feature is launched.
extern const char kLastAccuracyTipShown[];
extern const char kLastAccuracyTipShownDisabledUi[];
extern const char kPreviousInteractions[];
extern const char kPreviousInteractionsDisabledUi[];

}  // namespace prefs
}  // namespace accuracy_tips

#endif  // COMPONENTS_ACCURACY_TIPS_PREF_NAMES_H_
