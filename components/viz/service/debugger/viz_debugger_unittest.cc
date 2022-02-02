// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stddef.h>

#include <cstdio>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/vector2d_f.h"

#if VIZ_DEBUGGER_IS_ON()
using testing::_;
using testing::Mock;

namespace viz {

class VizDebuggerInternal : public VizDebugger {
 public:
  void ForceEnabled() { enabled_ = true; }
  bool Reset();

  int GetSourceCount() { return static_cast<int>(sources_.size()); }

  using VizDebugger::CallSubmitCommon;
  using VizDebugger::common_lock_;
  using VizDebugger::DrawCall;
  using VizDebugger::DrawTextCall;
  using VizDebugger::FrameAsJson;
  using VizDebugger::LogCall;
  using VizDebugger::UpdateFilters;
};

bool VizDebuggerInternal::Reset() {
  submission_count_ = 0;
  draw_rect_calls_.clear();
  draw_text_calls_.clear();
  logs_.clear();
  last_sent_source_count_ = 0;
  sources_.clear();
  return true;
}

namespace {

struct TestFilter {
  std::string anno;
  std::string func;
  std::string file;
  bool active = true;
  bool enabled = true;
};

static_assert(sizeof(VizDebuggerInternal) == sizeof(VizDebugger),
              "This test code exposes the internals of |VizDebugger| via an "
              "upcast; thus they must be the same size.");

class VisualDebuggerTest : public testing::Test {
 protected:
  VizDebuggerInternal* GetInternal() {
    return static_cast<VizDebuggerInternal*>(VizDebugger::GetInstance());
  }

  void SetUp() override { GetInternal()->Reset(); }
  void TearDown() override { GetInternal()->Reset(); }

  void SetFilter(std::vector<TestFilter> filters) {
    base::DictionaryValue filters_json;
    base::ListValue filters_list;
    for (auto&& each : filters) {
      base::DictionaryValue full_filter;
      base::DictionaryValue selector;
      if (!each.file.empty())
        selector.SetString("file", each.file);

      if (!each.func.empty())
        selector.SetString("func", each.func);

      selector.SetString("anno", each.anno);

      full_filter.SetKey("selector", std::move(selector));
      full_filter.SetBoolean("active", each.active);
      full_filter.SetBoolean("enabled", each.enabled);
      filters_list.Append(std::move(full_filter));
    }
    filters_json.SetKey("filters", std::move(filters_list));
    GetInternal()->FilterDebugStream(std::move(filters_json));
    GetInternal()->common_lock_.Acquire();
    GetInternal()->UpdateFilters();
    GetInternal()->common_lock_.Release();
  }

 public:
  struct StaticSource {
    std::string file;
    std::string func;
    std::string anno;
    int line;
    int index;
  };

  void GetFrameData() {
    sources_.clear();
    draw_calls_.clear();
    log_calls_.clear();
    draw_text_calls_.clear();
    GetInternal()->common_lock_.Acquire();
    absl::optional<base::Value> global_dict = GetInternal()->FrameAsJson(
        frame_counter_, gfx::Size(window_x_, window_y_), base::TimeTicks());
    GetInternal()->common_lock_.Release();
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

    for (size_t i = 0; i < list_source->GetListDeprecated().size(); i++) {
      auto&& local_dict = list_source->GetListDeprecated()[i];
      StaticSource ss;
      ss.file = local_dict.FindKey("file")->GetString();
      ss.func = local_dict.FindKey("func")->GetString();
      ss.anno = local_dict.FindKey("anno")->GetString();
      ss.line = local_dict.FindKey("line")->GetIfInt().value_or(kNoVal);
      ss.index = local_dict.FindKey("index")->GetIfInt().value_or(kNoVal);
      sources_.push_back(ss);
    }

    base::Value* draw_call_list = global_dict->FindListKey("drawcalls");
    EXPECT_TRUE(draw_call_list->is_list());

    auto func_common_call = [](const base::Value& dict, int* draw_index,
                               int* source_index,
                               VizDebugger::DrawOption* option) {
      *draw_index = dict.FindKey("drawindex")->GetIfInt().value_or(kNoVal);
      *source_index = dict.FindKey("source_index")->GetIfInt().value_or(kNoVal);

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

    for (size_t i = 0; i < draw_call_list->GetListDeprecated().size(); i++) {
      const base::Value& local_dict = draw_call_list->GetListDeprecated()[i];
      int draw_index;
      int source_index;
      VizDebugger::DrawOption option;
      func_common_call(local_dict, &draw_index, &source_index, &option);

      const base::Value* list_size = local_dict.FindListKey("size");
      EXPECT_TRUE(list_size->is_list());
      int size_x =
          list_size->GetListDeprecated()[0].GetIfInt().value_or(kNoVal);
      int size_y =
          list_size->GetListDeprecated()[1].GetIfInt().value_or(kNoVal);

      const base::Value* list_pos = local_dict.FindListKey("pos");
      EXPECT_TRUE(list_pos->is_list());
      float pos_x =
          list_pos->GetListDeprecated()[0].GetIfDouble().value_or(kNoVal);
      float pos_y =
          list_pos->GetListDeprecated()[1].GetIfDouble().value_or(kNoVal);

      VizDebuggerInternal::DrawCall draw_call(draw_index, source_index, option,
                                              gfx::Size(size_x, size_y),
                                              gfx::Vector2dF(pos_x, pos_y));

      draw_calls_.push_back(draw_call);
    }

    base::Value* text_call_list = global_dict->FindListKey("text");
    EXPECT_TRUE(text_call_list->is_list());

    for (size_t i = 0; i < text_call_list->GetListDeprecated().size(); i++) {
      const base::Value& local_dict = text_call_list->GetListDeprecated()[i];
      int draw_index;
      int source_index;
      VizDebugger::DrawOption option;

      func_common_call(local_dict, &draw_index, &source_index, &option);

      const base::Value* list_pos = local_dict.FindListKey("pos");
      EXPECT_TRUE(list_pos->is_list());
      float pos_x =
          list_pos->GetListDeprecated()[0].GetIfDouble().value_or(kNoVal);
      float pos_y =
          list_pos->GetListDeprecated()[1].GetIfDouble().value_or(kNoVal);

      VizDebuggerInternal::DrawTextCall text_call(
          draw_index, source_index, option, gfx::Vector2dF(pos_x, pos_y),
          local_dict.FindKey("text")->GetString());

      draw_text_calls_.push_back(text_call);
    }

    base::Value* log_call_list = global_dict->FindListKey("logs");
    EXPECT_TRUE(log_call_list->is_list());

    for (size_t i = 0; i < log_call_list->GetListDeprecated().size(); i++) {
      const base::Value& local_dict = log_call_list->GetListDeprecated()[i];
      int draw_index;
      int source_index;
      VizDebugger::DrawOption option;
      func_common_call(local_dict, &draw_index, &source_index, &option);

      VizDebuggerInternal::LogCall log_call(
          draw_index, source_index, option,
          local_dict.FindKey("value")->GetString());

      log_calls_.push_back(log_call);
    }
  }

  uint64_t frame_counter_ = 0;

  // Cached result of call to 'GetFrameData' to simplify code.
  uint64_t counter_;
  int window_x_ = 256;
  int window_y_ = 256;
  std::vector<StaticSource> sources_;
  std::vector<VizDebuggerInternal::DrawCall> draw_calls_;
  std::vector<VizDebuggerInternal::LogCall> log_calls_;
  std::vector<VizDebuggerInternal::DrawTextCall> draw_text_calls_;
};

TEST_F(VisualDebuggerTest, GeneralDrawSubmission) {
  const char kAnnoRect[] = "annorect";
  const char kAnnoText[] = "annotext";
  const char kAnnoLog[] = "annolog";
  const gfx::Rect kTestRect = gfx::Rect(12, 34, 56, 78);
  static const int kNumFrames = 4;
  GetInternal()->ForceEnabled();
  for (uint64_t frame_idx = 0; frame_idx < kNumFrames; frame_idx++) {
    SetFilter({TestFilter({""})});

    static const int kNumSubmission = 8;
    for (int i = 0; i < kNumSubmission; i++) {
      DBG_DRAW_RECT(kAnnoRect, kTestRect);
      DBG_DRAW_TEXT(kAnnoText, kTestRect.origin(),
                    base::StringPrintf("Text %d", i));
      DBG_LOG(kAnnoLog, "%d", i);
    }

    GetFrameData();

    EXPECT_EQ(counter_, frame_idx);
    EXPECT_EQ(window_x_, 256);
    EXPECT_EQ(window_x_, 256);
    EXPECT_EQ(draw_calls_.size(), static_cast<size_t>(kNumSubmission));
    EXPECT_EQ(log_calls_.size(), static_cast<size_t>(kNumSubmission));
    EXPECT_EQ(draw_text_calls_.size(), static_cast<size_t>(kNumSubmission));

    if (frame_idx == 0) {
      EXPECT_EQ(sources_.size(), 3u);
      EXPECT_EQ(sources_[0].func, "TestBody");
      EXPECT_EQ(sources_[0].file, __FILE__);
      EXPECT_EQ(sources_[0].anno, kAnnoRect);
      EXPECT_EQ(sources_[1].func, "TestBody");
      EXPECT_EQ(sources_[1].file, __FILE__);
      EXPECT_EQ(sources_[1].anno, kAnnoText);
      EXPECT_EQ(sources_[2].func, "TestBody");
      EXPECT_EQ(sources_[2].file, __FILE__);
      EXPECT_EQ(sources_[2].anno, kAnnoLog);
    } else {
      // After the first frame there are no new sources in the loop.
      EXPECT_EQ(sources_.size(), 0u);
    }

    for (int i = 0; i < kNumSubmission; i++) {
      EXPECT_EQ(draw_calls_[i].pos,
                gfx::Vector2dF(kTestRect.origin().x(), kTestRect.origin().y()));
      EXPECT_EQ(draw_calls_[i].obj_size, kTestRect.size());
      EXPECT_EQ(draw_calls_[i].source_index, 0);
      EXPECT_EQ(draw_calls_[i].draw_index, i * 3);

      EXPECT_EQ(draw_text_calls_[i].pos,
                gfx::Vector2dF(kTestRect.origin().x(), kTestRect.origin().y()));
      EXPECT_EQ(draw_text_calls_[i].source_index, 1);
      EXPECT_EQ(draw_text_calls_[i].draw_index, i * 3 + 1);
      EXPECT_EQ(draw_text_calls_[i].text, base::StringPrintf("Text %d", i));

      EXPECT_EQ(log_calls_[i].value, base::StringPrintf("%d", i));
      EXPECT_EQ(log_calls_[i].source_index, 2);
      EXPECT_EQ(log_calls_[i].draw_index, i * 3 + 2);
    }
  }
}

static void FunctionNameTest(const char* anno_rect, gfx::Rect rect) {
  DBG_DRAW_RECT(anno_rect, rect);
}

TEST_F(VisualDebuggerTest, FilterDrawSubmission) {
  const char kAnnoRect[] = "annorect";
  const char kAnnoMissing[] = "testmissing";
  const char kAnnoMatch[] = "before_annorect_after";

  GetInternal()->ForceEnabled();
  const gfx::Rect kTestRect = gfx::Rect(10, 30, 50, 70);
  const gfx::Rect kMissingRect = gfx::Rect(11, 33, 55, 77);
  std::vector<int> valid_indices;
  SetFilter({TestFilter({"annorect"})});
  valid_indices.push_back(GetInternal()->GetSourceCount());
  FunctionNameTest(kAnnoRect, kTestRect);
  valid_indices.push_back(GetInternal()->GetSourceCount());
  DBG_DRAW_RECT(kAnnoRect, kTestRect);
  DBG_DRAW_RECT(kAnnoMissing, kMissingRect);
  valid_indices.push_back(GetInternal()->GetSourceCount());
  DBG_DRAW_RECT(kAnnoMatch, kTestRect);

  SetFilter({TestFilter({"", "FunctionNameTest"})});
  DBG_DRAW_RECT(kAnnoRect, kMissingRect);
  valid_indices.push_back(0);
  FunctionNameTest(kAnnoRect, kTestRect);

  SetFilter({TestFilter({"", "TestBody"})});
  FunctionNameTest(kAnnoRect, kMissingRect);
  valid_indices.push_back(GetInternal()->GetSourceCount());
  DBG_DRAW_RECT(kAnnoRect, kTestRect);

  SetFilter({TestFilter({"", "", "no_file"})});
  DBG_DRAW_RECT(kAnnoRect, kMissingRect);

  SetFilter({TestFilter({"", "", __FILE__})});
  valid_indices.push_back(GetInternal()->GetSourceCount());
  DBG_DRAW_RECT(kAnnoRect, kTestRect);

  GetFrameData();
  EXPECT_EQ(sources_[0].func, "FunctionNameTest");
  EXPECT_EQ(sources_[0].file, __FILE__);
  EXPECT_EQ(sources_[0].anno, kAnnoRect);
  EXPECT_EQ(sources_[1].func, "TestBody");
  EXPECT_EQ(sources_[1].file, __FILE__);
  EXPECT_EQ(sources_[1].anno, kAnnoRect);
  EXPECT_EQ(sources_[2].anno, kAnnoMissing);
  EXPECT_EQ(sources_[3].anno, kAnnoMatch);

  auto check_draw = [](const VizDebuggerInternal::DrawCall& draw_call,
                       const gfx::Rect& rect, int src_idx, int draw_idx) {
    EXPECT_EQ(draw_call.pos,
              gfx::Vector2dF(rect.origin().x(), rect.origin().y()));
    EXPECT_EQ(draw_call.obj_size, rect.size());
    EXPECT_EQ(draw_call.source_index, src_idx);
    EXPECT_EQ(draw_call.draw_index, draw_idx);
  };

  // Makes sure all valid indices are here and have the correct rect.
  for (size_t i = 0; i < draw_calls_.size(); i++) {
    check_draw(draw_calls_[i], kTestRect, valid_indices[i], i);
  }
}

constexpr const char kTestFlagFunctionAnnoName[] = "testflagfunctionanno";

DBG_FLAG_FBOOL(kTestFlagFunctionAnnoName, check_flag_enabled)

static bool FlagFunctionTestEnable() {
  return check_flag_enabled();
}

TEST_F(VisualDebuggerTest, TestDebugFlagAnnoAndFunction) {
  GetInternal()->ForceEnabled();

  // Set our test flag to be disabled.
  SetFilter({TestFilter({kTestFlagFunctionAnnoName, "", "", true, false})});
  EXPECT_FALSE(FlagFunctionTestEnable());
  SetFilter({TestFilter({kTestFlagFunctionAnnoName, "", "", true, true})});
  EXPECT_TRUE(FlagFunctionTestEnable());
  SetFilter({TestFilter({kTestFlagFunctionAnnoName, "", "", true, false})});
  EXPECT_FALSE(FlagFunctionTestEnable());
}

// This tests makes sure that expensive string logging has no cost unless it is
// actively being filtered.
TEST_F(VisualDebuggerTest, NonFilterActiveNoCost) {
  GetInternal()->ForceEnabled();
  const char* kStrA = "anno_A";
  const char* kStrB = "anno_B";
  // These integers are mutated on a function invocation.
  int count_a = 0;
  int count_b = 0;

  auto get_a_string = [&count_a, &kStrA]() {
    count_a++;
    return std::string(kStrA);
  };
  auto get_b_string = [&count_b, &kStrB]() {
    count_b++;
    return std::string(kStrB);
  };

  // Filter on "anno_A" which should call 'get_a_string'.
  SetFilter({TestFilter({kStrA})});
  DBG_DRAW_TEXT(kStrA, gfx::Point(), get_a_string());
  DBG_DRAW_TEXT(kStrB, gfx::Point(), get_b_string());
  EXPECT_EQ(1, count_a);
  EXPECT_EQ(0, count_b);

  // Filter on "anno_B" which should call 'get_b_string'.
  SetFilter({TestFilter({kStrB})});
  DBG_DRAW_TEXT(kStrA, gfx::Point(), get_a_string());
  DBG_DRAW_TEXT(kStrB, gfx::Point(), get_b_string());
  EXPECT_EQ(1, count_a);
  EXPECT_EQ(1, count_b);
}

}  // namespace
}  // namespace viz
#else  // VIZ_DEBUGGER_IS_ON()

class VisualDebuggerTest : public testing::Test {};

DBG_FLAG_FBOOL("unit.test.fake.anno", flag_default_value_check)

TEST_F(VisualDebuggerTest, TestDebugFlagAnnoAndFunction) {
  // Visual debugger is disabled at build level this check should always return
  // false.
  EXPECT_FALSE(viz::VizDebugger::GetInstance()->IsEnabled());
  // The default value for a bool flag when the visual debugger is disabled is
  // false.
  EXPECT_FALSE(flag_default_value_check());
}

// For optimization purposes the flag fbool values return false as a constexpr.
// This allows the compiler to constant propagate and remove unused codepaths.
static_assert(flag_default_value_check() == false,
              "Default value when debugger is disabled is false.");

#endif
