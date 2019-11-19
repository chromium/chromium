// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/about_ui/credit_utils.h"

#include "base/files/file.h"
#include "components/about_ui/android/about_ui_jni_headers/CreditUtils_jni.h"

namespace about_ui {

static void JNI_CreditUtils_WriteCreditsHtml(JNIEnv* env, jint fd) {
  std::string html_content = GetCredits(false);
  base::File out_file(fd);
  out_file.WriteAtCurrentPos(html_content.c_str(), html_content.size());
}

}  // namespace about_ui
