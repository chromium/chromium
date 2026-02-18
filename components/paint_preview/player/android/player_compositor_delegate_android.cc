// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/player/android/player_compositor_delegate_android.h"

#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/unguessable_token_android.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/player/android/convert_to_java_bitmap.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/paint_preview/player/android/jni_headers/PlayerCompositorDelegateImpl_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace paint_preview {

namespace {

// To minimize peak memory usage limit the number of concurrent bitmap requests.
constexpr size_t kMaxParallelBitmapRequests = 3;
constexpr size_t kMaxParallelBitmapRequestsLowMemory = 2;

}  // namespace

static int64_t JNI_PlayerCompositorDelegateImpl_Initialize(
    JNIEnv* env,
    const JavaRef<jobject>& j_object,
    int64_t paint_preview_service,
    int64_t j_capture_result_ptr,
    const JavaRef<jstring>& j_url_spec,
    const JavaRef<jstring>& j_directory_key,
    bool j_main_frame_mode,
    const JavaRef<jobject>& j_compositor_error_callback,
    bool j_is_low_mem) {
  TRACE_EVENT0("paint_preview", "JNI_PlayerCompositorDelegateImpl_Initialize");
  PlayerCompositorDelegateAndroid* delegate =
      new PlayerCompositorDelegateAndroid(
          env, j_object,
          reinterpret_cast<PaintPreviewBaseService*>(paint_preview_service),
          j_capture_result_ptr, j_url_spec, j_directory_key, j_main_frame_mode,
          j_compositor_error_callback, j_is_low_mem);
  return reinterpret_cast<intptr_t>(delegate);
}

PlayerCompositorDelegateAndroid::PlayerCompositorDelegateAndroid(
    JNIEnv* env,
    const JavaRef<jobject>& j_object,
    PaintPreviewBaseService* paint_preview_service,
    int64_t j_capture_result_ptr,
    const JavaRef<jstring>& j_url_spec,
    const JavaRef<jstring>& j_directory_key,
    bool j_main_frame_mode,
    const JavaRef<jobject>& j_compositor_error_callback,
    bool j_is_low_mem)
    : PlayerCompositorDelegate(),
      request_id_(0),
      task_runner_(base::ThreadPool::CreateTaskRunner(
          {base::TaskPriority::USER_VISIBLE})),
      startup_timestamp_(base::TimeTicks::Now()) {
  std::string url_string;
  if (j_capture_result_ptr) {
    // Show@Startup doesn't use this.
    std::unique_ptr<CaptureResult> capture_result(
        reinterpret_cast<CaptureResult*>(j_capture_result_ptr));
    url_string = capture_result->proto.metadata().url();
    PlayerCompositorDelegate::SetCaptureResult(std::move(capture_result));
  } else {
    url_string = base::android::ConvertJavaStringToUTF8(env, j_url_spec);
  }

  PlayerCompositorDelegate::Initialize(
      paint_preview_service, GURL(url_string),
      DirectoryKey{
          base::android::ConvertJavaStringToUTF8(env, j_directory_key)},
      j_main_frame_mode,
      base::BindOnce(&base::android::RunIntCallbackAndroid,
                     ScopedJavaGlobalRef<jobject>(j_compositor_error_callback)),
      base::Seconds(15),
      (j_is_low_mem ? kMaxParallelBitmapRequestsLowMemory
                    : kMaxParallelBitmapRequests));

  java_ref_.Reset(env, j_object);
}

void PlayerCompositorDelegateAndroid::OnCompositorReady(
    CompositorStatus compositor_status,
    mojom::PaintPreviewBeginCompositeResponsePtr composite_response,
    float page_scale_factor,
    std::unique_ptr<ui::AXTreeUpdate> ax_tree) {
  TRACE_EVENT0("paint_preview",
               "PlayerCompositorDelegateAndroid::OnCompositorReady");
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
  std::vector<int32_t> scroll_extents;
  std::vector<int32_t> scroll_offsets;
  std::vector<int32_t> subframe_count;
  std::vector<base::UnguessableToken> subframe_ids;
  std::vector<int32_t> subframe_rects;
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

  Java_PlayerCompositorDelegateImpl_onCompositorReady(
      env, java_ref_, root_frame_guid, all_guids, scroll_extents,
      scroll_offsets, subframe_count, subframe_ids, subframe_rects,
      page_scale_factor, reinterpret_cast<intptr_t>(ax_tree.release()));
}

ScopedJavaLocalRef<jintArray>
PlayerCompositorDelegateAndroid::GetRootFrameOffsets(JNIEnv* env) {
  auto offsets = PlayerCompositorDelegate::GetRootFrameOffsets();
  ScopedJavaLocalRef<jintArray> j_offsets = base::android::ToJavaIntArray(
      env, std::vector<int>({offsets.x(), offsets.y()}));
  return j_offsets;
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

int32_t PlayerCompositorDelegateAndroid::RequestBitmap(
    JNIEnv* env,
    const std::optional<base::UnguessableToken>& frame_guid,
    const JavaRef<jobject>& j_bitmap_callback,
    base::OnceClosure&& error_callback,
    float j_scale_factor,
    int32_t j_clip_x,
    int32_t j_clip_y,
    int32_t j_clip_width,
    int32_t j_clip_height) {
  TRACE_EVENT0("paint_preview", "RequestBitmap");
  TRACE_EVENT_BEGIN("paint_preview",
                    "PlayerCompositorDelegateAndroid::RequestBitmap",
                    perfetto::Track(request_id_));
  gfx::Rect rect(j_clip_x, j_clip_y, j_clip_width, j_clip_height);
  auto callback = base::BindPostTask(
      task_runner_,
      base::BindOnce(&ConvertToJavaBitmap,
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &PlayerCompositorDelegateAndroid::OnJavaBitmapCallback,
                         weak_factory_.GetWeakPtr(),
                         ScopedJavaGlobalRef<jobject>(j_bitmap_callback),
                         std::move(error_callback), request_id_))));
  ++request_id_;

  // Callback can skip UI thread.
  return static_cast<int32_t>(
      PlayerCompositorDelegate::RequestBitmap(frame_guid, rect, j_scale_factor,
                                              std::move(callback)),
      /*run_callback_on_default_task_runner=*/false);
}

bool PlayerCompositorDelegateAndroid::CancelBitmapRequest(
    JNIEnv* env,
    int32_t j_request_id) {
  return static_cast<bool>(
      PlayerCompositorDelegate::CancelBitmapRequest(j_request_id));
}

void PlayerCompositorDelegateAndroid::CancelAllBitmapRequests(JNIEnv* env) {
  PlayerCompositorDelegate::CancelAllBitmapRequests();
}

void PlayerCompositorDelegateAndroid::OnJavaBitmapCallback(
    const ScopedJavaGlobalRef<jobject>& j_bitmap_callback,
    base::OnceClosure&& error_callback,
    int request_id,
    JavaBitmapResult result) {
  TRACE_EVENT0("paint_preview", "OnBitmapReceived");
  TRACE_EVENT_END(
      "paint_preview", /* PlayerCompositorDelegateAndroid::RequestBitmap */
      perfetto::Track(request_id), "status", static_cast<int>(result.status),
      "bytes", result.bytes);

  if (result.status ==
      mojom::PaintPreviewCompositor::BitmapStatus::kAllocFailed) {
    std::move(error_callback).Run();
    PlayerCompositorDelegate::OnAllocationFailure();
    return;
  }

  if (result.status != mojom::PaintPreviewCompositor::BitmapStatus::kSuccess) {
    std::move(error_callback).Run();
    return;
  }

  base::android::RunObjectCallbackAndroid(j_bitmap_callback,
                                          result.java_bitmap);

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
    const std::optional<base::UnguessableToken>& frame_guid,
    int32_t j_x,
    int32_t j_y) {
  if (!frame_guid.has_value()) {
    return jni_zero::g_empty_string.AsLocalRef(env);
  }
  auto res = PlayerCompositorDelegate::OnClick(
      frame_guid.value(),
      gfx::Rect(static_cast<int>(j_x), static_cast<int>(j_y), 1U, 1U));
  if (res.empty())
    return jni_zero::g_empty_string.AsLocalRef(env);

  base::UmaHistogramBoolean("Browser.PaintPreview.Player.LinkClicked", true);
  // TODO(crbug.com/40122441): Resolve cases where there are multiple links.
  // For now just return the first in the list.
  return base::android::ConvertUTF8ToJavaString(env, res[0]->spec());
}

void PlayerCompositorDelegateAndroid::SetCompressOnClose(
    JNIEnv* env,
    bool compress_on_close) {
  PlayerCompositorDelegate::SetCompressOnClose(compress_on_close);
}

void PlayerCompositorDelegateAndroid::Destroy(JNIEnv* env) {
  delete this;
}

PlayerCompositorDelegateAndroid::~PlayerCompositorDelegateAndroid() = default;

}  // namespace paint_preview

DEFINE_JNI(PlayerCompositorDelegateImpl)
