// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_PLAYER_ANDROID_PLAYER_COMPOSITOR_DELEGATE_ANDROID_H_
#define COMPONENTS_PAINT_PREVIEW_PLAYER_ANDROID_PLAYER_COMPOSITOR_DELEGATE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "components/paint_preview/player/player_compositor_delegate.h"

namespace paint_preview {
class PaintPreviewBaseService;
struct JavaBitmapResult;

class PlayerCompositorDelegateAndroid : public PlayerCompositorDelegate {
 public:
  PlayerCompositorDelegateAndroid(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_object,
      PaintPreviewBaseService* paint_preview_service,
      int64_t j_capture_result_ptr,
      const base::android::JavaRef<jstring>& j_url_spec,
      const base::android::JavaRef<jstring>& j_directory_key,
      bool j_main_frame_mode,
      const base::android::JavaRef<jobject>& j_compositor_error_callback,
      bool j_is_low_mem);

  void OnCompositorReady(
      CompositorStatus compositor_status,
      mojom::PaintPreviewBeginCompositeResponsePtr composite_response,
      float page_scale_factor,
      std::unique_ptr<ui::AXTreeUpdate> ax_tree) override;

  base::android::ScopedJavaLocalRef<jintArray> GetRootFrameOffsets(JNIEnv* env);

  // Called from Java when there is a request for a new bitmap. When the bitmap
  // is ready, it will be passed to j_bitmap_callback. In case of any failure,
  // j_error_callback will be called.
  int32_t RequestBitmap(
      JNIEnv* env,
      const std::optional<base::UnguessableToken>& frame_guid,
      const base::android::JavaRef<jobject>& j_bitmap_callback,
      base::OnceClosure&& error_callback,
      float j_scale_factor,
      int32_t j_clip_x,
      int32_t j_clip_y,
      int32_t j_clip_width,
      int32_t j_clip_height);

  bool CancelBitmapRequest(JNIEnv* env, int32_t j_request_id);

  void CancelAllBitmapRequests(JNIEnv* env);

  // Called from Java on touch event on a frame.
  base::android::ScopedJavaLocalRef<jstring> OnClick(
      JNIEnv* env,
      const std::optional<base::UnguessableToken>& frame_guid,
      int32_t j_x,
      int32_t j_y);

  // Called to set if compression should happen at close time.
  void SetCompressOnClose(JNIEnv* env, bool compress_on_close);

  void Destroy(JNIEnv* env);

  static void CompositeResponseFramesToVectors(
      const base::flat_map<base::UnguessableToken, mojom::FrameDataPtr>& frames,
      std::vector<base::UnguessableToken>* all_guids,
      std::vector<int>* scroll_extents,
      std::vector<int>* scroll_offsets,
      std::vector<int>* subframe_count,
      std::vector<base::UnguessableToken>* subframe_ids,
      std::vector<int>* subframe_rects);

 private:
  ~PlayerCompositorDelegateAndroid() override;

  void OnJavaBitmapCallback(
      const base::android::ScopedJavaGlobalRef<jobject>& j_bitmap_callback,
      base::OnceClosure&& error_callback,
      int request_id,
      JavaBitmapResult result);

  // Points to corresponding the Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  int request_id_;
  // Task runner for converting bitmaps allows parallel and not in order.
  scoped_refptr<base::TaskRunner> task_runner_;
  base::TimeTicks startup_timestamp_;

  base::WeakPtrFactory<PlayerCompositorDelegateAndroid> weak_factory_{this};

  PlayerCompositorDelegateAndroid(const PlayerCompositorDelegateAndroid&) =
      delete;
  PlayerCompositorDelegateAndroid& operator=(
      const PlayerCompositorDelegateAndroid&) = delete;
};  // namespace paint_preview

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_PLAYER_ANDROID_PLAYER_COMPOSITOR_DELEGATE_ANDROID_H_
