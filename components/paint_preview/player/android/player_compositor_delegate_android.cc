// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/player/android/player_compositor_delegate_android.h"

#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/unguessable_token_android.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
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

namespace {

ScopedJavaLocalRef<jobjectArray> ToJavaUnguessableTokenArray(
    JNIEnv* env,
    const std::vector<base::UnguessableToken>& tokens) {
  ScopedJavaLocalRef<jclass> j_unguessable_token_class =
      base::android::GetClass(env, "org/chromium/base/UnguessableToken");
  jobjectArray joa = env->NewObjectArray(
      tokens.size(), j_unguessable_token_class.obj(), nullptr);
  base::android::CheckException(env);

  for (size_t i = 0; i < tokens.size(); ++i) {
    ScopedJavaLocalRef<jobject> j_unguessable_token =
        base::android::UnguessableTokenAndroid::Create(env, tokens[i]);
    env->SetObjectArrayElement(joa, i, j_unguessable_token.obj());
  }

  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

ScopedJavaGlobalRef<jobject> ConvertToJavaBitmap(const SkBitmap& sk_bitmap) {
  return ScopedJavaGlobalRef<jobject>(
      gfx::ConvertToJavaBitmap(&sk_bitmap, gfx::OomBehavior::kReturnNullOnOom));
}

}  // namespace

jlong JNI_PlayerCompositorDelegateImpl_Initialize(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_object,
    jlong paint_preview_service,
    const JavaParamRef<jstring>& j_url_spec,
    const JavaParamRef<jstring>& j_directory_key,
    const JavaParamRef<jobject>& j_compositor_error_callback) {
  PlayerCompositorDelegateAndroid* delegate =
      new PlayerCompositorDelegateAndroid(
          env, j_object,
          reinterpret_cast<PaintPreviewBaseService*>(paint_preview_service),
          j_url_spec, j_directory_key, j_compositor_error_callback);
  return reinterpret_cast<intptr_t>(delegate);
}

PlayerCompositorDelegateAndroid::PlayerCompositorDelegateAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_object,
    PaintPreviewBaseService* paint_preview_service,
    const JavaParamRef<jstring>& j_url_spec,
    const JavaParamRef<jstring>& j_directory_key,
    const JavaParamRef<jobject>& j_compositor_error_callback)
    : PlayerCompositorDelegate(),
      request_id_(0),
      startup_timestamp_(base::TimeTicks::Now()) {
  PlayerCompositorDelegate::Initialize(
      paint_preview_service,
      GURL(base::android::ConvertJavaStringToUTF8(env, j_url_spec)),
      DirectoryKey{
          base::android::ConvertJavaStringToUTF8(env, j_directory_key)},
      base::BindOnce(&base::android::RunIntCallbackAndroid,
                     ScopedJavaGlobalRef<jobject>(j_compositor_error_callback)),
      base::TimeDelta::FromSeconds(15));
  java_ref_.Reset(env, j_object);
}

void PlayerCompositorDelegateAndroid::OnCompositorReady(
    CompositorStatus compositor_status,
    mojom::PaintPreviewBeginCompositeResponsePtr composite_response) {
  bool compositor_started = CompositorStatus::OK == compositor_status;
  base::UmaHistogramBoolean(
      "Browser.PaintPreview.Player.CompositorProcessStartedCorrectly",
      compositor_started);
  if (!compositor_started) {
    DLOG(ERROR) << "Compositor process failed to begin with code: "
                << static_cast<int>(compositor_status);
    if (compositor_error_)
      std::move(compositor_error_).Run(static_cast<int>(compositor_status));

    return;
  }
  auto delta = base::TimeTicks::Now() - startup_timestamp_;
  if (delta.InMicroseconds() >= 0) {
    base::UmaHistogramTimes(
        "Browser.PaintPreview.Player.CompositorProcessStartupTime", delta);
  }
  JNIEnv* env = base::android::AttachCurrentThread();

  std::vector<base::UnguessableToken> all_guids;
  std::vector<int> scroll_extents;
  std::vector<int> scroll_offsets;
  std::vector<int> subframe_count;
  std::vector<base::UnguessableToken> subframe_ids;
  std::vector<int> subframe_rects;
  base::UnguessableToken root_frame_guid;

  if (composite_response) {
    CompositeResponseFramesToVectors(
        composite_response->frames, &all_guids, &scroll_extents,
        &scroll_offsets, &subframe_count, &subframe_ids, &subframe_rects);
    root_frame_guid = composite_response->root_frame_guid;
  } else {
    // If there is no composite response due to a failure we don't have a root
    // frame GUID to pass. However, the token cannot be null so create a
    // placeholder.
    root_frame_guid = base::UnguessableToken::Create();
  }

  ScopedJavaLocalRef<jobjectArray> j_all_guids =
      ToJavaUnguessableTokenArray(env, all_guids);
  ScopedJavaLocalRef<jintArray> j_scroll_extents =
      base::android::ToJavaIntArray(env, scroll_extents);
  ScopedJavaLocalRef<jintArray> j_scroll_offsets =
      base::android::ToJavaIntArray(env, scroll_offsets);
  ScopedJavaLocalRef<jintArray> j_subframe_count =
      base::android::ToJavaIntArray(env, subframe_count);
  ScopedJavaLocalRef<jobjectArray> j_subframe_ids =
      ToJavaUnguessableTokenArray(env, subframe_ids);
  ScopedJavaLocalRef<jintArray> j_subframe_rects =
      base::android::ToJavaIntArray(env, subframe_rects);
  ScopedJavaLocalRef<jobject> j_root_frame_guid =
      base::android::UnguessableTokenAndroid::Create(env, root_frame_guid);

  Java_PlayerCompositorDelegateImpl_onCompositorReady(
      env, java_ref_, j_root_frame_guid, j_all_guids, j_scroll_extents,
      j_scroll_offsets, j_subframe_count, j_subframe_ids, j_subframe_rects);
}

// static
void PlayerCompositorDelegateAndroid::CompositeResponseFramesToVectors(
    const base::flat_map<base::UnguessableToken, mojom::FrameDataPtr>& frames,
    std::vector<base::UnguessableToken>* all_guids,
    std::vector<int>* scroll_extents,
    std::vector<int>* scroll_offsets,
    std::vector<int>* subframe_count,
    std::vector<base::UnguessableToken>* subframe_ids,
    std::vector<int>* subframe_rects) {
  all_guids->reserve(frames.size());
  scroll_extents->reserve(2 * frames.size());
  subframe_count->reserve(frames.size());
  int all_subframes_count = 0;
  for (const auto& pair : frames) {
    all_guids->push_back(pair.first);
    scroll_extents->push_back(pair.second->scroll_extents.width());
    scroll_extents->push_back(pair.second->scroll_extents.height());
    scroll_offsets->push_back(pair.second->scroll_offsets.width());
    scroll_offsets->push_back(pair.second->scroll_offsets.height());
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
    const JavaParamRef<jobject>& j_frame_guid,
    const JavaParamRef<jobject>& j_bitmap_callback,
    const JavaParamRef<jobject>& j_error_callback,
    jfloat j_scale_factor,
    jint j_clip_x,
    jint j_clip_y,
    jint j_clip_width,
    jint j_clip_height) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "paint_preview", "PlayerCompositorDelegateAndroid::RequestBitmap",
      TRACE_ID_LOCAL(request_id_));

  PlayerCompositorDelegate::RequestBitmap(
      base::android::UnguessableTokenAndroid::FromJavaUnguessableToken(
          env, j_frame_guid),
      gfx::Rect(j_clip_x, j_clip_y, j_clip_width, j_clip_height),
      j_scale_factor,
      base::BindOnce(&PlayerCompositorDelegateAndroid::OnBitmapCallback,
                     weak_factory_.GetWeakPtr(),
                     ScopedJavaGlobalRef<jobject>(j_bitmap_callback),
                     ScopedJavaGlobalRef<jobject>(j_error_callback),
                     request_id_));
  ++request_id_;
}

void PlayerCompositorDelegateAndroid::OnBitmapCallback(
    const ScopedJavaGlobalRef<jobject>& j_bitmap_callback,
    const ScopedJavaGlobalRef<jobject>& j_error_callback,
    int request_id,
    mojom::PaintPreviewCompositor::BitmapStatus status,
    const SkBitmap& sk_bitmap) {
  TRACE_EVENT_NESTABLE_ASYNC_END2(
      "paint_preview", "PlayerCompositorDelegateAndroid::RequestBitmap",
      TRACE_ID_LOCAL(request_id), "status", static_cast<int>(status), "bytes",
      sk_bitmap.computeByteSize());

  if (status != mojom::PaintPreviewCompositor::BitmapStatus::kSuccess ||
      sk_bitmap.isNull()) {
    base::android::RunRunnableAndroid(j_error_callback);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ConvertToJavaBitmap, sk_bitmap),
      base::BindOnce(base::BindOnce(
          [](const ScopedJavaGlobalRef<jobject>& j_bitmap_callback,
             const ScopedJavaGlobalRef<jobject>& j_error_callback,
             const ScopedJavaGlobalRef<jobject>& j_bitmap) {
            if (!j_bitmap) {
              base::android::RunRunnableAndroid(j_error_callback);
              return;
            }
            base::android::RunObjectCallbackAndroid(j_bitmap_callback,
                                                    j_bitmap);
          },
          j_bitmap_callback, j_error_callback)));

  if (request_id == 0) {
    auto delta = base::TimeTicks::Now() - startup_timestamp_;
    if (delta.InMicroseconds() >= 0) {
      base::UmaHistogramTimes("Browser.PaintPreview.Player.TimeToFirstBitmap",
                              delta);
    }
  }
}

ScopedJavaLocalRef<jstring> PlayerCompositorDelegateAndroid::OnClick(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_frame_guid,
    jint j_x,
    jint j_y) {
  auto res = PlayerCompositorDelegate::OnClick(
      base::android::UnguessableTokenAndroid::FromJavaUnguessableToken(
          env, j_frame_guid),
      gfx::Rect(static_cast<int>(j_x), static_cast<int>(j_y), 1U, 1U));
  if (res.empty())
    return base::android::ConvertUTF8ToJavaString(env, "");

  base::UmaHistogramBoolean("Browser.PaintPreview.Player.LinkClicked", true);
  // TODO(crbug/1061435): Resolve cases where there are multiple links.
  // For now just return the first in the list.
  return base::android::ConvertUTF8ToJavaString(env, res[0]->spec());
}

void PlayerCompositorDelegateAndroid::SetCompressOnClose(
    JNIEnv* env,
    jboolean compress_on_close) {
  PlayerCompositorDelegate::SetCompressOnClose(
      static_cast<bool>(compress_on_close));
}

void PlayerCompositorDelegateAndroid::Destroy(JNIEnv* env) {
  delete this;
}

PlayerCompositorDelegateAndroid::~PlayerCompositorDelegateAndroid() = default;

}  // namespace paint_preview
