// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_OMNIBOX_OMNIBOX_URL_EMPHASIZER_H_
#define CHROME_BROWSER_UI_ANDROID_OMNIBOX_OMNIBOX_URL_EMPHASIZER_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

// Helper functions for the Omnibox URL emphasizer on Android.
class OmniboxUrlEmphasizer {
 public:
  OmniboxUrlEmphasizer() = delete;
  OmniboxUrlEmphasizer(const OmniboxUrlEmphasizer&) = delete;
  OmniboxUrlEmphasizer& operator=(const OmniboxUrlEmphasizer&) = delete;
};

#endif  // CHROME_BROWSER_UI_ANDROID_OMNIBOX_OMNIBOX_URL_EMPHASIZER_H_
