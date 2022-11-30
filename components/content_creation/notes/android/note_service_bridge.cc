// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/android/note_service_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "components/content_creation/notes/android/jni_headers/NoteServiceBridge_jni.h"
#include "components/content_creation/notes/android/note_template_conversion_bridge.h"
#include "components/content_creation/notes/core/server/note_data.h"
#include "components/content_creation/notes/core/templates/note_template.h"

namespace content_creation {
namespace {
using ::base::android::AttachCurrentThread;
using ::base::android::ConvertJavaStringToUTF8;
using ::base::android::ConvertUTF8ToJavaString;

const char kNoteServiceBridgeKey[] = "note_service_bridge";

void RunGetTemplatesCallback(const JavaRef<jobject>& j_callback,
                             std::vector<NoteTemplate> templates) {
  JNIEnv* env = AttachCurrentThread();
  RunObjectCallbackAndroid(
      j_callback,
      NoteTemplateConversionBridge::CreateJavaNoteTemplates(env, templates));
}

void RunPublishNoteCallback(const JavaRef<jobject>& j_callback,
                            std::string noteUrl) {
  JNIEnv* env = AttachCurrentThread();
  RunObjectCallbackAndroid(j_callback, ConvertUTF8ToJavaString(env, noteUrl));
}

}  // namespace

// static
ScopedJavaLocalRef<jobject> NoteServiceBridge::GetBridgeForNoteService(
    NoteService* note_service) {
  DCHECK(note_service);
  if (!note_service->GetUserData(kNoteServiceBridgeKey)) {
    note_service->SetUserData(
        kNoteServiceBridgeKey,
        std::make_unique<NoteServiceBridge>(note_service));
  }

  NoteServiceBridge* bridge = static_cast<NoteServiceBridge*>(
      note_service->GetUserData(kNoteServiceBridgeKey));
  return ScopedJavaLocalRef<jobject>(bridge->java_obj_);
}

NoteServiceBridge::NoteServiceBridge(NoteService* note_service)
    : note_service_(note_service) {
  DCHECK(note_service_);
  JNIEnv* env = AttachCurrentThread();
  java_obj_.Reset(
      env, Java_NoteServiceBridge_create(env, reinterpret_cast<int64_t>(this))
               .obj());
}

NoteServiceBridge::~NoteServiceBridge() {
  JNIEnv* env = AttachCurrentThread();
  Java_NoteServiceBridge_clearNativePtr(env, java_obj_);
}

void NoteServiceBridge::GetTemplates(JNIEnv* env,
                                     const JavaParamRef<jobject>& jcaller,
                                     const JavaParamRef<jobject>& jcallback) {
  note_service_->GetTemplates(base::BindOnce(
      &RunGetTemplatesCallback, ScopedJavaGlobalRef<jobject>(jcallback)));
}

jboolean NoteServiceBridge::IsPublishAvailable(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  return note_service_->IsPublishAvailable();
}

void NoteServiceBridge::PublishNote(JNIEnv* env,
                                    const JavaParamRef<jobject>& jcaller,
                                    jstring selectedText,
                                    jstring shareUrl,
                                    const JavaParamRef<jobject>& jcallback) {
  NoteData noteData(ConvertJavaStringToUTF8(env, selectedText),
                    ConvertJavaStringToUTF8(env, shareUrl));

  note_service_->PublishNote(
      noteData, base::BindOnce(&RunPublishNoteCallback,
                               ScopedJavaGlobalRef<jobject>(jcallback)));
}

}  // namespace content_creation
