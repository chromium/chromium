// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/debugger/viz_debugger_unittests/viz_debugger_unittest_base.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "third_party/skia/include/codec/SkCodec.h"
#include "third_party/skia/include/codec/SkPngDecoder.h"
#include "ui/gfx/geometry/rect_f.h"

#if VIZ_DEBUGGER_IS_ON()

namespace viz {

TestFilter::TestFilter() = default;
TestFilter::~TestFilter() = default;

TestFilter::TestFilter(const std::string& anno_) : anno(anno_) {}

TestFilter::TestFilter(const std::string& anno_, const std::string& func_)
    : anno(anno_), func(func_) {}

TestFilter::TestFilter(const std::string& anno_,
                       const std::string& func_,
                       const std::string& file_)
    : anno(anno_), func(func_), file(file_) {}

TestFilter::TestFilter(const std::string& anno_,
                       const std::string& func_,
                       const std::string& file_,
                       const bool& active_,
                       const bool& enabled_)
    : anno(anno_),
      func(func_),
      file(file_),
      active(active_),
      enabled(enabled_) {}

TestFilter::TestFilter(const TestFilter& other) = default;

StaticSource::StaticSource() = default;
StaticSource::~StaticSource() = default;
StaticSource::StaticSource(const StaticSource& other) = default;

VizDebuggerInternal* VisualDebuggerTestBase::GetInternal() {
  return static_cast<VizDebuggerInternal*>(VizDebugger::GetInstance());
}

void VisualDebuggerTestBase::SetUp() {
  GetInternal()->Reset();
}
void VisualDebuggerTestBase::TearDown() {
  GetInternal()->Reset();
}

VisualDebuggerTestBase::VisualDebuggerTestBase() = default;
VisualDebuggerTestBase::~VisualDebuggerTestBase() = default;

void VisualDebuggerTestBase::SetFilter(std::vector<TestFilter> filters) {
  base::Value::List filters_list;
  for (auto&& each : filters) {
    auto selector = base::Value::Dict().Set("anno", each.anno);
    if (!each.file.empty())
      selector.Set("file", each.file);

    if (!each.func.empty())
      selector.Set("func", each.func);

    filters_list.Append(base::Value::Dict()
                            .Set("selector", std::move(selector))
                            .Set("active", each.active)
                            .Set("enabled", each.enabled));
  }

  GetInternal()->FilterDebugStream(
      base::Value::Dict().Set("filters", std::move(filters_list)));
  GetInternal()->GetRWLock()->WriteLock();
  GetInternal()->UpdateFilters();
  GetInternal()->GetRWLock()->WriteUnLock();
}

void VisualDebuggerTestBase::GetFrameData(bool clear_cache) {
  if (clear_cache) {
    sources_cache_.clear();
    draw_calls_cache_.clear();
    log_calls_cache_.clear();
    buffers_.clear();
  }

  GetInternal()->GetRWLock()->WriteLock();
  size_t const kNumDrawCallSubmission = static_cast<size_t>(std::min(
      GetInternal()->GetRectCallsTailIdx(), GetInternal()->GetRectCallsSize()));
  size_t const kNumLogSubmission = static_cast<size_t>(
      std::min(GetInternal()->GetLogsTailIdx(), GetInternal()->GetLogsSize()));

  std::optional<base::Value> maybe_global_dict_val = GetInternal()->FrameAsJson(
      frame_counter_, gfx::Size(window_x_, window_y_), base::TimeTicks());
  EXPECT_TRUE(maybe_global_dict_val);
  EXPECT_TRUE(maybe_global_dict_val->is_dict());
  const base::Value::Dict& global_dict = maybe_global_dict_val->GetDict();

  GetInternal()->GetRWLock()->WriteUnLock();
  frame_counter_++;

  base::StringToUint64(global_dict.FindString("frame")->c_str(), &counter_);
  static const int kNoVal = -1;
  int expected_version = global_dict.FindInt("version").value_or(kNoVal);
  // Check to update these unit tests if a backwards compatible change has
  // been made.
  EXPECT_EQ(1, expected_version);

  window_x_ = global_dict.FindInt("windowx").value_or(kNoVal);
  window_y_ = global_dict.FindInt("windowy").value_or(kNoVal);

  const base::Value::List* list_source = global_dict.FindList("new_sources");
  EXPECT_TRUE(list_source);

  for (const auto& local_dict_val : *list_source) {
    const base::Value::Dict& local_dict = local_dict_val.GetDict();
    StaticSource ss;
    ss.file = *local_dict.FindString("file");
    ss.func = *local_dict.FindString("func");
    ss.anno = *local_dict.FindString("anno");
    ss.line = local_dict.FindInt("line").value_or(kNoVal);
    ss.index = local_dict.FindInt("index").value_or(kNoVal);
    sources_cache_.push_back(ss);
  }

  const base::Value::List* draw_call_list = global_dict.FindList("drawcalls");
  EXPECT_TRUE(draw_call_list);

  auto func_common_call = [](const base::Value::Dict& dict, int* draw_index,
                             int* source_index, int* thread_id,
                             VizDebugger::DrawOption* option) {
    *draw_index = dict.FindInt("drawindex").value_or(kNoVal);
    *source_index = dict.FindInt("source_index").value_or(kNoVal);
    *thread_id = dict.FindInt("thread_id").value_or(kNoVal);

    const base::Value::Dict* option_dict = dict.FindDict("option");

    uint32_t red;
    uint32_t green;
    uint32_t blue;
    std::sscanf(option_dict->FindString("color")->c_str(), "#%x%x%x", &red,
                &green, &blue);

    option->color_r = red;
    option->color_g = green;
    option->color_b = blue;
    option->color_a =
        static_cast<uint8_t>(option_dict->FindInt("alpha").value_or(kNoVal));
  };

  for (size_t i = 0; i < kNumDrawCallSubmission; i++) {
    const base::Value::Dict& local_dict = (*draw_call_list)[i].GetDict();
    int draw_index;
    int source_index;
    int thread_id;
    VizDebugger::DrawOption option;
    func_common_call(local_dict, &draw_index, &source_index, &thread_id,
                     &option);

    const base::Value::List* list_size = local_dict.FindList("size");
    EXPECT_TRUE(list_size);
    float size_x = (*list_size)[0].GetIfDouble().value_or(kNoVal);
    float size_y = (*list_size)[1].GetIfDouble().value_or(kNoVal);

    const base::Value::List* list_pos = local_dict.FindList("pos");
    EXPECT_TRUE(list_pos);
    float pos_x = (*list_pos)[0].GetIfDouble().value_or(kNoVal);
    float pos_y = (*list_pos)[1].GetIfDouble().value_or(kNoVal);

    float uv_pos_x = 0.0;
    float uv_pos_y = 0.0;
    float uv_size_w = 1.0;
    float uv_size_h = 1.0;
    const base::Value::List* list_uv_pos = local_dict.FindList("uv_pos");
    const base::Value::List* list_uv_size = local_dict.FindList("uv_size");
    if (list_uv_pos && list_uv_size) {
      uv_pos_x = (*list_uv_pos)[0].GetIfDouble().value_or(0.0f);
      uv_pos_y = (*list_uv_pos)[1].GetIfDouble().value_or(0.0f);

      uv_size_w = (*list_uv_size)[0].GetIfDouble().value_or(1.0f);
      uv_size_h = (*list_uv_size)[1].GetIfDouble().value_or(1.0f);
    }

    const std::optional<int> buffer_id = local_dict.FindInt("buff_id");
    const std::string* text_str = local_dict.FindString("text");
    VizDebuggerInternal::DrawCall draw_call(
        draw_index, source_index, thread_id, option, gfx::SizeF(size_x, size_y),
        gfx::Vector2dF(pos_x, pos_y), buffer_id ? buffer_id.value() : -1,
        gfx::RectF(uv_pos_x, uv_pos_y, uv_size_w, uv_size_h),
        text_str ? (*text_str) : std::string());

    draw_calls_cache_.push_back(draw_call);
  }

  const base::Value::Dict* buffer_map_dict = global_dict.FindDict("buff_map");
  if (buffer_map_dict) {
    for (base::Value::Dict::const_iterator itr = buffer_map_dict->begin();
         itr != buffer_map_dict->end(); itr++) {
      EXPECT_TRUE(itr->second.is_string());
      const std::string& image_data_uri = itr->second.GetString();

      // We expect |buffer_map_dict| contains base64-encoded data URIs. We only
      // need to look at the base64-encoded data since |SkCodec| can decode
      // without extra metadata about the image.
      constexpr const char* kDataUriPrefix = "data:image/png;base64,";
      EXPECT_TRUE(image_data_uri.starts_with(kDataUriPrefix));
      std::string_view image_base64_encoded =
          std::string_view(image_data_uri).substr(strlen(kDataUriPrefix));
      const std::optional<std::vector<uint8_t>> image_bytes =
          base::Base64Decode(image_base64_encoded);
      EXPECT_TRUE(image_bytes.has_value());

      // |MakeWithoutCopy| is safe because we expect to be done decoding |data|
      // into |buff.bitmap| before we release |image_bytes|.
      sk_sp<SkData> data =
          SkData::MakeWithoutCopy(image_bytes->data(), image_bytes->size());
      SkCodec::Result decode_result;
      std::unique_ptr<SkCodec> codec =
          SkPngDecoder::Decode(data, &decode_result);
      EXPECT_EQ(SkCodec::Result::kSuccess, decode_result);

      VizDebuggerInternal::BufferInfo buff;
      buff.bitmap.allocPixels(codec->getInfo());
      const SkCodec::Result read_result =
          codec->getPixels(buff.bitmap.pixmap());
      EXPECT_EQ(SkCodec::Result::kSuccess, read_result);

      int id;
      base::StringToInt(itr->first, &id);
      VizDebuggerInternal::Buffer buffer;
      buffer.id = id;
      buffer.buffer_info = buff;
      buffers_.push_back(buffer);
    }
  }

  const base::Value::List* log_call_list = global_dict.FindList("logs");
  EXPECT_TRUE(log_call_list);

  for (size_t i = 0; i < kNumLogSubmission; i++) {
    const base::Value::Dict& local_dict = (*log_call_list)[i].GetDict();
    int draw_index;
    int source_index;
    int thread_id;
    VizDebugger::DrawOption option;
    func_common_call(local_dict, &draw_index, &source_index, &thread_id,
                     &option);

    VizDebuggerInternal::LogCall log_call(draw_index, source_index, thread_id,
                                          option,
                                          *local_dict.FindString("value"));

    log_calls_cache_.push_back(log_call);
  }
}
}  // namespace viz

#endif  // VIZ_DEBUGGER_IS_ON()
