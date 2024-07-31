// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/paint_preview/player/android/javatests/paint_preview_test_service.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/threading/thread_restrictions.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/browser/test_paint_preview_policy.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/file_utils.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/version.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/paint_preview/player/android/javatests_jni_headers/PaintPreviewTestService_jni.h"

using base::android::JavaParamRef;

namespace paint_preview {

const char kPaintPreviewDir[] = "paint_preview";
const char kTestDirName[] = "PaintPreviewTestService";
constexpr int kSquareSideLen = 50;

namespace {

void CreateBackground(SkCanvas* canvas,
                      const SkColor& color,
                      uint32_t width,
                      uint32_t height) {
  SkPaint paint;
  paint.setColor(SK_ColorWHITE);
  canvas->drawRect(SkRect::MakeWH(width, height), paint);
  paint.setColor(color);
  for (uint32_t j = 0; j * kSquareSideLen < height; ++j) {
    for (uint32_t i = (j % 2); i * kSquareSideLen < width; i += 2) {
      canvas->drawRect(SkRect::MakeXYWH(i * kSquareSideLen, j * kSquareSideLen,
                                        kSquareSideLen, kSquareSideLen),
                       paint);
    }
  }
}

void AddLinks(SkCanvas* canvas,
              PaintPreviewFrameProto& frame,
              const std::vector<std::string> link_urls,
              const std::vector<int> link_rects) {
  SkPaint paint;
  paint.setColor(SK_ColorCYAN);
  for (size_t i = 0; i < link_urls.size(); ++i) {
    auto* link_proto = frame.add_links();
    link_proto->set_url(link_urls[i]);
    auto* rect = link_proto->mutable_rect();
    int x = link_rects[i * 4];
    int y = link_rects[i * 4 + 1];
    int width = link_rects[i * 4 + 2];
    int height = link_rects[i * 4 + 3];
    rect->set_x(x);
    rect->set_y(y);
    rect->set_width(width);
    rect->set_height(height);
    canvas->drawRect(SkRect::MakeXYWH(x, y, width, height), paint);
  }
}

bool WriteSkp(sk_sp<SkPicture> skp,
              const base::FilePath& skp_path,
              PictureSerializationContext* pctx) {
  FileWStream wstream(base::File(
      skp_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE));
  TypefaceUsageMap typeface_map;
  TypefaceSerializationContext tctx(&typeface_map);
  ImageSerializationContext ictx;
  auto procs = MakeSerialProcs(pctx, &tctx, &ictx);
  skp->serialize(&wstream, &procs);
  wstream.Close();
  if (wstream.DidWriteFail()) {
    LOG(ERROR) << "SKP Write failed";
    return false;
  }
  LOG(INFO) << "Wrote SKP " << wstream.ActualBytesWritten() << " bytes";
  return true;
}

}  // namespace

jlong JNI_PaintPreviewTestService_GetInstance(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_path) {
  base::FilePath file_path(base::android::ConvertJavaStringToUTF8(env, j_path));
  PaintPreviewTestService* service = new PaintPreviewTestService(file_path);
  return reinterpret_cast<intptr_t>(service);
}

PaintPreviewTestService::PaintPreviewTestService(const base::FilePath& path)
    : PaintPreviewBaseService(
          std::make_unique<PaintPreviewFileMixin>(path, kTestDirName),
          std::make_unique<TestPaintPreviewPolicy>(),
          false),
      test_data_dir_(
          path.AppendASCII(kPaintPreviewDir).AppendASCII(kTestDirName)) {}

PaintPreviewTestService::~PaintPreviewTestService() = default;

jlong PaintPreviewTestService::GetBaseService(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(
      static_cast<PaintPreviewBaseService*>(this));
}

base::android::ScopedJavaLocalRef<jintArray>
PaintPreviewTestService::CreateSingleSkp(
    JNIEnv* env,
    jint j_id,
    jint j_width,
    jint j_height,
    const JavaParamRef<jintArray>& j_link_rects,
    const JavaParamRef<jobjectArray>& j_link_urls,
    const JavaParamRef<jintArray>& j_child_rects) {
  const int id = static_cast<int>(j_id);
  uint32_t width = static_cast<uint32_t>(j_width);
  uint32_t height = static_cast<uint32_t>(j_height);

  if (id == 0)
    frames_.emplace(0, Frame(base::UnguessableToken::Create()));

  std::vector<int> out;
  auto it = frames_.find(id);
  if (it == frames_.end()) {
    LOG(ERROR) << "Unexpected frame ID.";
    return base::android::ToJavaIntArray(env, out);
  }
  auto* data = &it->second;

  SkPictureRecorder recorder;
  auto* canvas = recorder.beginRecording(SkRect::MakeWH(width, height));
  SkColor color;
  if (id == 0) {
    color = SK_ColorGRAY;
  } else {
    constexpr SkColor colors[4] = {SK_ColorRED, SK_ColorBLUE, SK_ColorGREEN,
                                   SK_ColorMAGENTA};
    color = colors[id % 4];
  }
  CreateBackground(canvas, color, width, height);

  {
    // Scope the reference to |frame| here to prevent using when |data| may be
    // invalidated downstream.
    PaintPreviewFrameProto& frame = data->proto;
    frame.set_embedding_token_low(data->guid.GetLowForSerialization());
    frame.set_embedding_token_high(data->guid.GetHighForSerialization());
    frame.set_is_main_frame(id == 0);
    // No initial offset.
    frame.set_scroll_offset_x(0);
    frame.set_scroll_offset_y(0);

    std::vector<std::string> link_urls;
    base::android::AppendJavaStringArrayToStringVector(env, j_link_urls,
                                                       &link_urls);
    std::vector<int> link_rects;
    base::android::JavaIntArrayToIntVector(env, j_link_rects, &link_rects);
    AddLinks(canvas, frame, link_urls, link_rects);
  }

  std::vector<int> child_rects;
  base::android::JavaIntArrayToIntVector(env, j_child_rects, &child_rects);

  // Add sub pictures.
  for (size_t i = 0; i < child_rects.size() / 4; ++i) {
    const int x = child_rects[i * 4];
    const int y = child_rects[i * 4 + 1];
    const int w = child_rects[i * 4 + 2];
    const int h = child_rects[i * 4 + 3];
    auto rect = SkRect::MakeXYWH(x, y, w, h);
    auto sub_pic = SkPicture::MakePlaceholder(rect);
    SkMatrix matrix = SkMatrix::Translate(x, y);
    uint32_t sub_id = sub_pic->uniqueID();
    canvas->drawPicture(sub_pic, &matrix, nullptr);

    out.push_back(sub_id);

    auto token = base::UnguessableToken::Create();
    {
      auto* id_map_entry = data->proto.add_content_id_to_embedding_tokens();
      id_map_entry->set_embedding_token_low(token.GetLowForSerialization());
      id_map_entry->set_embedding_token_high(token.GetHighForSerialization());
      id_map_entry->set_content_id(sub_id);
    }

    frames_.emplace(sub_id, Frame(token));
    // Re-find |data| as it may have moved when emplacing the new frame.
    it = frames_.find(id);
    data = &it->second;
    data->ctx.content_id_to_embedding_token.insert(
        std::make_pair(sub_id, token));
    data->ctx.content_id_to_transformed_clip.insert(
        std::make_pair(sub_id, rect));
  }

  data->skp = recorder.finishRecordingAsPicture();
  return base::android::ToJavaIntArray(env, out);
}

jboolean PaintPreviewTestService::SerializeFrames(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_key,
    const base::android::JavaParamRef<jstring>& j_url) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!base::PathExists(test_data_dir_)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(test_data_dir_, &error)) {
      LOG(ERROR) << "Failed to create dir: "
                 << base::File::ErrorToString(error);
      return false;
    }
  }
  base::FilePath path = test_data_dir_.AppendASCII(
      base::android::ConvertJavaStringToUTF8(env, j_key));
  if (!base::CreateDirectory(path)) {
    LOG(ERROR) << "Failed to create directory.";
    return false;
  }

  auto root_it = frames_.find(0);
  if (root_it == frames_.end()) {
    LOG(ERROR) << "No root frame";
    return false;
  }

  PaintPreviewProto paint_preview;
  auto* metadata = paint_preview.mutable_metadata();
  metadata->set_url(base::android::ConvertJavaStringToUTF8(env, j_url));
  metadata->set_version(kPaintPreviewVersion);

  auto skp_path =
      path.AppendASCII(base::StrCat({root_it->second.guid.ToString(), ".skp"}));
  root_it->second.proto.set_file_path(skp_path.AsUTF8Unsafe());
  *paint_preview.mutable_root_frame() = root_it->second.proto;
  if (!WriteSkp(root_it->second.skp, skp_path, &(root_it->second.ctx)))
    return false;

  for (auto it = frames_.begin(); it != frames_.end(); ++it) {
    if (it->first == 0)
      continue;

    skp_path =
        path.AppendASCII(base::StrCat({it->second.guid.ToString(), ".skp"}));
    it->second.proto.set_file_path(skp_path.AsUTF8Unsafe());
    *paint_preview.add_subframes() = it->second.proto;
    if (!WriteSkp(it->second.skp, skp_path, &(it->second.ctx)))
      return false;
  }

  if (!WriteProtoToFile(path.AppendASCII("proto.pb"), paint_preview)) {
    LOG(ERROR) << "Failed to write proto to file.";
    return false;
  }
  frames_.clear();
  return true;
}

PaintPreviewTestService::Frame::Frame(const base::UnguessableToken& token)
    : guid(token) {}
PaintPreviewTestService::Frame::~Frame() = default;

PaintPreviewTestService::Frame::Frame(Frame&& rhs) noexcept = default;
PaintPreviewTestService::Frame& PaintPreviewTestService::Frame::operator=(
    Frame&& rhs) noexcept = default;

}  // namespace paint_preview
