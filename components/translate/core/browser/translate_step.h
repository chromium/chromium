// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_STEP_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_STEP_H_

#include "build/build_config.h"

namespace translate {

// Denotes which state the user is in with respect to translate.
enum TranslateStep {
  TRANSLATE_STEP_BEFORE_TRANSLATE,
  TRANSLATE_STEP_TRANSLATING,
  TRANSLATE_STEP_AFTER_TRANSLATE,
#if BUILDFLAG(IS_IOS)
  TRANSLATE_STEP_NEVER_TRANSLATE,
#endif
  TRANSLATE_STEP_TRANSLATE_ERROR
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_STEP_H_
