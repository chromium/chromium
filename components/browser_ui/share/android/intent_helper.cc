// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/share/android/intent_helper.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"

// Must come after headers that provide symbols used by @JniType.
#include "components/browser_ui/share/android/jni_headers/IntentHelper_jni.h"

using jni_zero::AttachCurrentThread;

namespace browser_ui {

void SendEmail(const std::u16string& d_email,
               const std::u16string& d_subject,
               const std::u16string& d_body,
               const std::u16string& d_chooser_title,
               const std::u16string& d_file_to_attach) {
  JNIEnv* env = AttachCurrentThread();
  Java_IntentHelper_sendEmail(env, d_email, d_subject, d_body, d_chooser_title,
                              d_file_to_attach);
}

}  // namespace browser_ui

DEFINE_JNI(IntentHelper)
