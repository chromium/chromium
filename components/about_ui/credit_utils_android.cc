// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/about_ui/credit_utils.h"

#include "base/containers/span.h"
#include "base/files/file.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/about_ui/android/about_ui_jni_headers/CreditUtils_jni.h"

namespace about_ui {

static void JNI_CreditUtils_WriteCreditsHtml(JNIEnv* env, jint fd) {
  base::File out_file(fd);
  out_file.WriteAtCurrentPos(base::as_byte_span(GetCredits(false)));
}

}  // namespace about_ui
