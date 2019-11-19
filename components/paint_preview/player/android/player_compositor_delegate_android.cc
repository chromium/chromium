// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/player/android/player_compositor_delegate_android.h"

#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "components/paint_preview/player/android/jni_headers/PlayerCompositorDelegateImpl_jni.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace paint_preview {

jlong JNI_PlayerCompositorDelegateImpl_Initialize(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_object,
    const JavaParamRef<jstring>& j_string_url) {
  PlayerCompositorDelegateAndroid* mediator =
      new PlayerCompositorDelegateAndroid(env, j_object, j_string_url);
  return reinterpret_cast<intptr_t>(mediator);
}

PlayerCompositorDelegateAndroid::PlayerCompositorDelegateAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_object,
    const JavaParamRef<jstring>& j_string_url)
    : PlayerCompositorDelegate(
          GURL(base::android::ConvertJavaStringToUTF16(env, j_string_url))) {
  java_ref_.Reset(env, j_object);
}

void PlayerCompositorDelegateAndroid::OnCompositorReady(
    const mojom::PaintPreviewBeginCompositeResponse& composite_response) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // We use int64_t instead of uint64_t because (i) there is no equivalent
  // type to uint64_t in Java and (ii) base::android::ToJavaLongArray accepts
  // a std::vector<int64_t> as input.
  std::vector<int64_t> all_guids;
  std::vector<int> scroll_extents;
  std::vector<int> subframe_count;
  std::vector<int64_t> subframe_ids;
  std::vector<int> subframe_rects;

  CompositeResponseFramesToVectors(composite_response.frames, &all_guids,
                                   &scroll_extents, &subframe_count,
                                   &subframe_ids, &subframe_rects);

  ScopedJavaLocalRef<jlongArray> j_all_guids =
      base::android::ToJavaLongArray(env, all_guids);
  ScopedJavaLocalRef<jintArray> j_scroll_extents =
      base::android::ToJavaIntArray(env, scroll_extents);
  ScopedJavaLocalRef<jintArray> j_subframe_count =
      base::android::ToJavaIntArray(env, subframe_count);
  ScopedJavaLocalRef<jlongArray> j_subframe_ids =
      base::android::ToJavaLongArray(env, subframe_ids);
  ScopedJavaLocalRef<jintArray> j_subframe_rects =
      base::android::ToJavaIntArray(env, subframe_rects);

  Java_PlayerCompositorDelegateImpl_onCompositorReady(
      env, java_ref_, composite_response.root_frame_guid, j_all_guids,
      j_scroll_extents, j_subframe_count, j_subframe_ids, j_subframe_rects);
}

// static
void PlayerCompositorDelegateAndroid::CompositeResponseFramesToVectors(
    const base::flat_map<uint64_t, mojom::FrameDataPtr>& frames,
    std::vector<int64_t>* all_guids,
    std::vector<int>* scroll_extents,
    std::vector<int>* subframe_count,
    std::vector<int64_t>* subframe_ids,
    std::vector<int>* subframe_rects) {
  all_guids->reserve(frames.size());
  scroll_extents->reserve(2 * frames.size());
  subframe_count->reserve(frames.size());
  int all_subframes_count = 0;
  for (const auto& pair : frames) {
    all_guids->push_back(pair.first);
    scroll_extents->push_back(pair.second->scroll_extents.width());
    scroll_extents->push_back(pair.second->scroll_extents.height());
    subframe_count->push_back(pair.second->subframes.size());
    all_subframes_count += pair.second->subframes.size();
  }

  subframe_ids->reserve(all_subframes_count);
  subframe_rects->reserve(4 * all_subframes_count);
  for (const auto& pair : frames) {
    for (const auto& subframe : pair.second->subframes) {
      subframe_ids->push_back(subframe->frame_guid);
      subframe_rects->push_back(subframe->clip_rect.x());
      subframe_rects->push_back(subframe->clip_rect.y());
      subframe_rects->push_back(subframe->clip_rect.width());
      subframe_rects->push_back(subframe->clip_rect.height());
    }
  }
}

void PlayerCompositorDelegateAndroid::RequestBitmap(
    JNIEnv* env,
    jlong j_frame_guid,
    const JavaParamRef<jobject>& j_bitmap_callback,
    const JavaParamRef<jobject>& j_error_callback,
    jfloat j_scale_factor,
    jint j_clip_x,
    jint j_clip_y,
    jint j_clip_width,
    jint j_clip_height) {
  gfx::Rect clip_rect =
      gfx::Rect(j_clip_x, j_clip_y, j_clip_width, j_clip_height);
  PlayerCompositorDelegate::RequestBitmap(
      j_frame_guid, clip_rect, j_scale_factor,
      base::BindOnce(&PlayerCompositorDelegateAndroid::OnBitmapCallback,
                     weak_factory_.GetWeakPtr(),
                     ScopedJavaGlobalRef<jobject>(j_bitmap_callback),
                     ScopedJavaGlobalRef<jobject>(j_error_callback)));
}

void PlayerCompositorDelegateAndroid::OnBitmapCallback(
    const ScopedJavaGlobalRef<jobject>& j_bitmap_callback,
    const ScopedJavaGlobalRef<jobject>& j_error_callback,
    mojom::PaintPreviewCompositor::Status status,
    const SkBitmap& sk_bitmap) {
  if (status == mojom::PaintPreviewCompositor::Status::kSuccess) {
    base::android::RunObjectCallbackAndroid(
        j_bitmap_callback, gfx::ConvertToJavaBitmap(&sk_bitmap));
  } else {
    base::android::RunRunnableAndroid(j_error_callback);
  }
}

void PlayerCompositorDelegateAndroid::OnClick(JNIEnv* env,
                                              jlong j_frame_guid,
                                              jint j_x,
                                              jint j_y) {
  PlayerCompositorDelegate::OnClick(j_frame_guid, j_x, j_y);
}

void PlayerCompositorDelegateAndroid::Destroy(JNIEnv* env) {
  delete this;
}

PlayerCompositorDelegateAndroid::~PlayerCompositorDelegateAndroid() = default;

}  // namespace paint_preview
