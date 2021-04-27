// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/android/note_template_conversion_bridge.h"

#include "base/android/jni_string.h"
#include "components/content_creation/notes/android/jni_headers/NoteTemplateConversionBridge_jni.h"

namespace content_creation {

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace {

ScopedJavaLocalRef<jobject> CreateJavaTemplateAndMaybeAddToList(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject> jlist,
    const NoteTemplate& template) {
  return Java_NoteTemplateConversionBridge_createTemplateAndMaybeAddToList(
      env, jlist, ConvertUTF8ToJavaString(template.localized_name));
}

}  // namespace

// static
ScopedJavaLocalRef<jobject>
NoteTemplateConversionBridge::CreateJavaNoteTemplates(
    JNIEnv* env,
    const std::vector<NoteTemplate>& templates) {
  ScopedJavaLocalRef<jobject> jlist =
      Java_NoteTemplateConversionBridge_createTemplateList(env);

  for (const auto& template : templates) {
    CreateJavaTemplateAndMaybeAddToList(env, jlist, templates);
  }

  return jlist;
}

}  // namespace content_creation
