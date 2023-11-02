// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_PLAYER_ANDROID_JAVATESTS_PAINT_PREVIEW_TEST_SERVICE_H_
#define COMPONENTS_PAINT_PREVIEW_PLAYER_ANDROID_JAVATESTS_PAINT_PREVIEW_TEST_SERVICE_H_

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/serial_utils.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace paint_preview {

// A simple implementation of PaintPreviewBaseService used in tests.
class PaintPreviewTestService : public PaintPreviewBaseService {
 public:
  PaintPreviewTestService(const base::FilePath& path);
  ~PaintPreviewTestService() override;

  PaintPreviewTestService(const PaintPreviewTestService&) = delete;
  PaintPreviewTestService& operator=(const PaintPreviewTestService&) = delete;

  jlong GetBaseService(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jintArray> CreateSingleSkp(
      JNIEnv* env,
      jint j_id,
      jint j_width,
      jint j_height,
      const base::android::JavaParamRef<jintArray>& j_link_rects,
      const base::android::JavaParamRef<jobjectArray>& j_link_urls,
      const base::android::JavaParamRef<jintArray>& j_child_rects);

  jboolean SerializeFrames(JNIEnv* env,
                           const base::android::JavaParamRef<jstring>& j_key,
                           const base::android::JavaParamRef<jstring>& j_url);

 private:
  struct Frame {
    explicit Frame(const base::UnguessableToken& token);
    ~Frame();

    Frame(const Frame& rhs) = delete;
    Frame& operator=(const Frame& rhs) = delete;
    Frame(Frame&& rhs) noexcept;
    Frame& operator=(Frame&& rhs) noexcept;

    base::UnguessableToken guid;
    sk_sp<SkPicture> skp;
    PaintPreviewFrameProto proto;
    PictureSerializationContext ctx;
  };

  base::FilePath test_data_dir_;
  base::flat_map<uint32_t, Frame> frames_;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_PLAYER_ANDROID_JAVATESTS_PAINT_PREVIEW_TEST_SERVICE_H_
