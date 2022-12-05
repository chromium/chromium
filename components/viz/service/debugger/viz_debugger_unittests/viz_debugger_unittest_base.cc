// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/debugger/viz_debugger_unittests/viz_debugger_unittest_base.h"

#include <algorithm>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
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
  base::Value::Dict filters_json;
  base::Value::List filters_list;
  for (auto&& each : filters) {
    base::Value::Dict full_filter;
    base::Value::Dict selector;
    if (!each.file.empty())
      selector.Set("file", each.file);

    if (!each.func.empty())
      selector.Set("func", each.func);

    selector.Set("anno", each.anno);

    full_filter.Set("selector", std::move(selector));
    full_filter.Set("active", each.active);
    full_filter.Set("enabled", each.enabled);
    filters_list.Append(std::move(full_filter));
  }
  filters_json.Set("filters", std::move(filters_list));
  GetInternal()->FilterDebugStream(base::Value(std::move(filters_json)));
  GetInternal()->GetRWLock()->WriteLock();
  GetInternal()->UpdateFilters();
  GetInternal()->GetRWLock()->WriteUnLock();
}

void VisualDebuggerTestBase::GetFrameData(bool clear_cache) {
  if (clear_cache) {
    sources_cache_.clear();
    draw_rect_calls_cache_.clear();
    log_calls_cache_.clear();
    draw_text_calls_cache_.clear();
    buffers_.clear();
  }

  GetInternal()->GetRWLock()->WriteLock();
  size_t const kNumDrawCallSubmission = static_cast<size_t>(std::min(
      GetInternal()->GetRectCallsTailIdx(), GetInternal()->GetRectCallsSize()));
  size_t const kNumTextCallSubmission = static_cast<size_t>(std::min(
      GetInternal()->GetTextCallsTailIdx(), GetInternal()->GetTextCallsSize()));
  size_t const kNumLogSubmission = static_cast<size_t>(
      std::min(GetInternal()->GetLogsTailIdx(), GetInternal()->GetLogsSize()));

  absl::optional<base::Value> global_dict = GetInternal()->FrameAsJson(
      frame_counter_, gfx::Size(window_x_, window_y_), base::TimeTicks());
  GetInternal()->GetRWLock()->WriteUnLock();
  frame_counter_++;

  EXPECT_TRUE(global_dict->is_dict());

  base::StringToUint64(global_dict->FindKey("frame")->GetString().c_str(),
                       &counter_);
  static const int kNoVal = -1;
  int expected_version =
      global_dict->FindKey("version")->GetIfInt().value_or(kNoVal);
  // Check to update these unit tests if a backwards compatible change has
  // been made.
  EXPECT_EQ(1, expected_version);

  window_x_ = global_dict->FindKey("windowx")->GetIfInt().value_or(kNoVal);
  window_y_ = global_dict->FindKey("windowy")->GetIfInt().value_or(kNoVal);

  base::Value* list_source = global_dict->FindListKey("new_sources");
  EXPECT_TRUE(list_source->is_list());

  for (const auto& local_dict : list_source->GetList()) {
    StaticSource ss;
    ss.file = local_dict.FindKey("file")->GetString();
    ss.func = local_dict.FindKey("func")->GetString();
    ss.anno = local_dict.FindKey("anno")->GetString();
    ss.line = local_dict.FindKey("line")->GetIfInt().value_or(kNoVal);
    ss.index = local_dict.FindKey("index")->GetIfInt().value_or(kNoVal);
    sources_cache_.push_back(ss);
  }

  base::Value* draw_call_list = global_dict->FindListKey("drawcalls");
  EXPECT_TRUE(draw_call_list->is_list());

  auto func_common_call = [](const base::Value& dict, int* draw_index,
                             int* source_index, int* thread_id,
                             VizDebugger::DrawOption* option) {
    *draw_index = dict.FindKey("drawindex")->GetIfInt().value_or(kNoVal);
    *source_index = dict.FindKey("source_index")->GetIfInt().value_or(kNoVal);
    *thread_id = dict.FindKey("thread_id")->GetIfInt().value_or(kNoVal);

    const base::Value* option_dict = dict.FindDictKey("option");

    uint32_t red;
    uint32_t green;
    uint32_t blue;
    std::sscanf(option_dict->FindKey("color")->GetString().c_str(), "#%x%x%x",
                &red, &green, &blue);

    option->color_r = red;
    option->color_g = green;
    option->color_b = blue;
    option->color_a = static_cast<uint8_t>(
        option_dict->FindKey("alpha")->GetIfInt().value_or(kNoVal));
  };

  for (size_t i = 0; i < kNumDrawCallSubmission; i++) {
    const base::Value& local_dict = draw_call_list->GetList()[i];
    int draw_index;
    int source_index;
    int thread_id;
    VizDebugger::DrawOption option;
    func_common_call(local_dict, &draw_index, &source_index, &thread_id,
                     &option);

    const base::Value* list_size = local_dict.FindListKey("size");
    EXPECT_TRUE(list_size->is_list());
    int size_x = list_size->GetList()[0].GetIfInt().value_or(kNoVal);
    int size_y = list_size->GetList()[1].GetIfInt().value_or(kNoVal);

    const base::Value* list_pos = local_dict.FindListKey("pos");
    EXPECT_TRUE(list_pos->is_list());
    float pos_x = list_pos->GetList()[0].GetIfDouble().value_or(kNoVal);
    float pos_y = list_pos->GetList()[1].GetIfDouble().value_or(kNoVal);

    const base::Value* buffer_id = local_dict.FindKey("buff_id");

    float uv_pos_x = 0.0;
    float uv_pos_y = 0.0;
    float uv_size_w = 1.0;
    float uv_size_h = 1.0;
    const base::Value* list_uv_pos = local_dict.FindListKey("uv_pos");
    const base::Value* list_uv_size = local_dict.FindListKey("uv_size");
    if (local_dict.FindListKey("uv_pos")->GetIfList() &&
        local_dict.FindListKey("uv_size")->GetIfList()) {
      EXPECT_TRUE(list_uv_pos->is_list());
      uv_pos_x = list_uv_pos->GetList()[0].GetIfDouble().value_or(0.0f);
      uv_pos_y = list_uv_pos->GetList()[1].GetIfDouble().value_or(0.0f);

      EXPECT_TRUE(list_uv_size->is_list());
      uv_size_w = list_uv_size->GetList()[0].GetIfDouble().value_or(1.0f);
      uv_size_h = list_uv_size->GetList()[1].GetIfDouble().value_or(1.0f);
    }

    VizDebuggerInternal::DrawCall draw_call(
        draw_index, source_index, thread_id, option, gfx::Size(size_x, size_y),
        gfx::Vector2dF(pos_x, pos_y), buffer_id ? buffer_id->GetInt() : -1,
        gfx::RectF(uv_pos_x, uv_pos_y, uv_size_w, uv_size_h));

    draw_rect_calls_cache_.push_back(draw_call);
  }

  base::Value* buffer_map_dict = global_dict->FindKey("buff_map");
  if (buffer_map_dict) {
    for (base::Value::Dict::iterator itr = buffer_map_dict->GetDict().begin();
         itr != buffer_map_dict->GetDict().end(); itr++) {
      base::Value* buffer_dict = buffer_map_dict->FindKey(itr->first);
      EXPECT_TRUE(buffer_dict);
      int width = buffer_dict->FindKey("width")->GetIfInt().value_or(kNoVal);
      int height = buffer_dict->FindKey("height")->GetIfInt().value_or(kNoVal);
      base::Value* buffer_info = buffer_dict->FindKey("buffer");
      EXPECT_TRUE(buffer_info->is_list());
      VizDebuggerInternal::BufferInfo buff;
      buff.bitmap.setInfo(SkImageInfo::Make(
          width, height, kBGRA_8888_SkColorType, kUnpremul_SkAlphaType));
      buff.bitmap.allocPixels();
      for (size_t i = 0; i < buffer_info->GetList().size() / 4; i++) {
        uint8_t temp1 = buffer_info->GetList()[i * 4].GetInt();
        uint8_t temp2 = buffer_info->GetList()[i * 4 + 1].GetInt();
        uint8_t temp3 = buffer_info->GetList()[i * 4 + 2].GetInt();
        uint8_t temp4 = buffer_info->GetList()[i * 4 + 3].GetInt();
        *buff.bitmap.getAddr32(i % width, i / width) =
            SkColorSetARGB(temp4, temp1, temp2, temp3);
      }
      int id;
      base::StringToInt(itr->first, &id);
      VizDebuggerInternal::Buffer buffer;
      buffer.id = id;
      buffer.buffer_info = buff;
      buffers_.push_back(buffer);
    }
  }

  base::Value* text_call_list = global_dict->FindListKey("text");
  EXPECT_TRUE(text_call_list->is_list());

  for (size_t i = 0; i < kNumTextCallSubmission; i++) {
    const base::Value& local_dict = text_call_list->GetList()[i];
    int draw_index;
    int source_index;
    int thread_id;
    VizDebugger::DrawOption option;

    func_common_call(local_dict, &draw_index, &source_index, &thread_id,
                     &option);

    const base::Value* list_pos = local_dict.FindListKey("pos");
    EXPECT_TRUE(list_pos->is_list());
    float pos_x = list_pos->GetList()[0].GetIfDouble().value_or(kNoVal);
    float pos_y = list_pos->GetList()[1].GetIfDouble().value_or(kNoVal);

    VizDebuggerInternal::DrawTextCall text_call(
        draw_index, source_index, thread_id, option,
        gfx::Vector2dF(pos_x, pos_y), local_dict.FindKey("text")->GetString());

    draw_text_calls_cache_.push_back(text_call);
  }

  base::Value* log_call_list = global_dict->FindListKey("logs");
  EXPECT_TRUE(log_call_list->is_list());

  for (size_t i = 0; i < kNumLogSubmission; i++) {
    const base::Value& local_dict = log_call_list->GetList()[i];
    int draw_index;
    int source_index;
    int thread_id;
    VizDebugger::DrawOption option;
    func_common_call(local_dict, &draw_index, &source_index, &thread_id,
                     &option);

    VizDebuggerInternal::LogCall log_call(
        draw_index, source_index, thread_id, option,
        local_dict.FindKey("value")->GetString());

    log_calls_cache_.push_back(log_call);
  }
}
}  // namespace viz

#endif  // VIZ_DEBUGGER_IS_ON()
