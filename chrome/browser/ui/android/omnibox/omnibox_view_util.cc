// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/omnibox/omnibox_view_util.h"

#include "base/android/jni_string.h"
#include "components/omnibox/browser/omnibox_text_util.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/ui/android/omnibox/jni_headers/OmniboxViewUtil_jni.h"

// static
static std::u16string JNI_OmniboxViewUtil_SanitizeTextForPaste(
    const std::u16string& text) {
  return omnibox::SanitizeTextForPaste(text);
}

DEFINE_JNI(OmniboxViewUtil)
