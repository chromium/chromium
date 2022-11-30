// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_NOTES_ANDROID_NOTE_TEMPLATE_CONVERSION_BRIDGE_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_ANDROID_NOTE_TEMPLATE_CONVERSION_BRIDGE_H_

#include "base/android/jni_android.h"
#include "components/content_creation/notes/core/templates/note_template.h"

using base::android::ScopedJavaLocalRef;

namespace content_creation {

class NoteTemplateConversionBridge {
 public:
  static ScopedJavaLocalRef<jobject> CreateJavaNoteTemplates(
      JNIEnv* env,
      const std::vector<NoteTemplate>& templates);
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_ANDROID_NOTE_TEMPLATE_CONVERSION_BRIDGE_H_
