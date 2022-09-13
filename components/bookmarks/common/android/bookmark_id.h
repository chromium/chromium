// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_COMMON_ANDROID_BOOKMARK_ID_H_
#define COMPONENTS_BOOKMARKS_COMMON_ANDROID_BOOKMARK_ID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

namespace bookmarks {
namespace android {

// See BookmarkId#getId
long JavaBookmarkIdGetId(JNIEnv* env,
                         const base::android::JavaRef<jobject>& obj);

// See BookmarkId#getType
int JavaBookmarkIdGetType(JNIEnv* env,
                          const base::android::JavaRef<jobject>& obj);

// See BookmarkId#createBookmarkId
base::android::ScopedJavaLocalRef<jobject> JavaBookmarkIdCreateBookmarkId(
    JNIEnv* env, jlong id, jint type);

}  // namespace android
}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_COMMON_ANDROID_BOOKMARK_ID_H_
