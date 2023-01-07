// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_NOTES_ANDROID_NOTE_SERVICE_BRIDGE_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_ANDROID_NOTE_SERVICE_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/content_creation/notes/core/note_service.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace content_creation {

class NoteServiceBridge : public base::SupportsUserData::Data {
 public:
  static ScopedJavaLocalRef<jobject> GetBridgeForNoteService(
      NoteService* note_service);

  explicit NoteServiceBridge(NoteService* note_service);
  ~NoteServiceBridge() override;

  // Not copyable or movable.
  NoteServiceBridge(const NoteServiceBridge&) = delete;
  NoteServiceBridge& operator=(const NoteServiceBridge&) = delete;

  void GetTemplates(JNIEnv* env,
                    const JavaParamRef<jobject>& jcaller,
                    const JavaParamRef<jobject>& jcallback);

  jboolean IsPublishAvailable(JNIEnv* env,
                              const JavaParamRef<jobject>& jcaller);

  void PublishNote(JNIEnv* env,
                   const JavaParamRef<jobject>& jcaller,
                   jstring selectedText,
                   jstring shareUrl,
                   const JavaParamRef<jobject>& jcallback);

 private:
  ScopedJavaGlobalRef<jobject> java_obj_;

  // Not owned.
  raw_ptr<NoteService> note_service_;
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_ANDROID_NOTE_SERVICE_BRIDGE_H_
