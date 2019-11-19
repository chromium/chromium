// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/common/android/bookmark_id.h"

#include "components/bookmarks/common/android/bookmarks_jni_headers/BookmarkId_jni.h"

namespace bookmarks {
namespace android {

long JavaBookmarkIdGetId(JNIEnv* env,
                         const base::android::JavaRef<jobject>& obj) {
  return Java_BookmarkId_getId(env, obj);
}

int JavaBookmarkIdGetType(JNIEnv* env,
                          const base::android::JavaRef<jobject>& obj) {
  return Java_BookmarkId_getType(env, obj);
}

base::android::ScopedJavaLocalRef<jobject> JavaBookmarkIdCreateBookmarkId(
    JNIEnv* env, jlong id, jint type) {
  return Java_BookmarkId_createBookmarkId(env, id, type);
}

}  // namespace android
}  // namespace bookmarks
