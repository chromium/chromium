// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_PREF_NAMES_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_PREF_NAMES_H_

namespace prefs {

// A pref of the boolean value which indicates whether the On Device Translator
// API is allowed. This pref is set per profile by the "TranslatorAPIAllowed"
// Enterprise policy.
extern const char kTranslatorAPIAllowed[];

}  // namespace prefs

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_PREF_NAMES_H_
