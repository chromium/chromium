// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <cstdio>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/debugger/viz_debugger_unittests/viz_debugger_internal.h"
#include "components/viz/service/debugger/viz_debugger_unittests/viz_debugger_unittest_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/vector2d_f.h"

#if VIZ_DEBUGGER_IS_ON()
using testing::_;
using testing::Mock;

namespace viz {

namespace {

static_assert(sizeof(VizDebuggerInternal) == sizeof(VizDebugger),
              "This test code exposes the internals of |VizDebugger| via an "
              "upcast; thus they must be the same size.");

class VisualDebuggerTest : public VisualDebuggerTestBase {};

TEST_F(VisualDebuggerTest, GeneralDrawSubmission) {
  const char kAnnoRect[] = "annorect";
  const char kAnnoText[] = "annotext";
  const char kAnnoLog[] = "annolog";
  const gfx::Rect kTestRect = gfx::Rect(12, 34, 56, 78);
  const gfx::RectF kTestUV = gfx::RectF(0.46, 0.25, 0.38, 1);
  static const int kNumFrames = 4;
  GetInternal()->ForceEnabled();
  for (uint64_t frame_idx = 0; frame_idx < kNumFrames; frame_idx++) {
    SetFilter({TestFilter("")});

    static const int kNumSubmission = 8;
    for (int i = 0; i < kNumSubmission; i++) {
      int buff_id = i;
      DBG_DRAW_RECT_BUFF_UV(kAnnoRect, kTestRect, &buff_id, kTestUV);
      DBG_DRAW_TEXT(kAnnoText, kTestRect.origin(),
                    base::StringPrintf("Text %d", i));
      DBG_LOG(kAnnoLog, "%d", i);
    }

    GetFrameData(true);
    EXPECT_EQ(counter_, frame_idx);
    EXPECT_EQ(window_x_, 256);
    EXPECT_EQ(window_x_, 256);
    EXPECT_EQ(static_cast<int>(draw_rect_calls_cache_.size()), kNumSubmission);
    EXPECT_EQ(static_cast<int>(log_calls_cache_.size()), kNumSubmission);
    EXPECT_EQ(static_cast<int>(draw_text_calls_cache_.size()), kNumSubmission);

    if (frame_idx == 0) {
      EXPECT_EQ(sources_cache_.size(), 3u);
      EXPECT_EQ(sources_cache_[0].func, "TestBody");
      EXPECT_EQ(sources_cache_[0].file, __FILE__);
      EXPECT_EQ(sources_cache_[0].anno, kAnnoRect);
      EXPECT_EQ(sources_cache_[1].func, "TestBody");
      EXPECT_EQ(sources_cache_[1].file, __FILE__);
      EXPECT_EQ(sources_cache_[1].anno, kAnnoText);
      EXPECT_EQ(sources_cache_[2].func, "TestBody");
      EXPECT_EQ(sources_cache_[2].file, __FILE__);
      EXPECT_EQ(sources_cache_[2].anno, kAnnoLog);
    } else {
      // After the first frame there are no new sources in the loop.
      EXPECT_EQ(sources_cache_.size(), 0u);
    }

    for (int i = 0; i < kNumSubmission; i++) {
      EXPECT_EQ(draw_rect_calls_cache_[i].uv, kTestUV);
      EXPECT_EQ(draw_rect_calls_cache_[i].pos,
                gfx::Vector2dF(kTestRect.origin().x(), kTestRect.origin().y()));
      EXPECT_EQ(draw_rect_calls_cache_[i].obj_size, kTestRect.size());
      EXPECT_EQ(draw_rect_calls_cache_[i].source_index, 0);
      EXPECT_EQ(draw_rect_calls_cache_[i].draw_index, i * 3);

      EXPECT_EQ(draw_text_calls_cache_[i].pos,
                gfx::Vector2dF(kTestRect.origin().x(), kTestRect.origin().y()));
      EXPECT_EQ(draw_text_calls_cache_[i].source_index, 1);
      EXPECT_EQ(draw_text_calls_cache_[i].draw_index, i * 3 + 1);
      EXPECT_EQ(draw_text_calls_cache_[i].text,
                base::StringPrintf("Text %d", i));

      EXPECT_EQ(log_calls_cache_[i].value, base::StringPrintf("%d", i));
      EXPECT_EQ(log_calls_cache_[i].source_index, 2);
      EXPECT_EQ(log_calls_cache_[i].draw_index, i * 3 + 2);
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
  SetFilter({TestFilter("annorect")});
  valid_indices.push_back(GetInternal()->GetSourceCount());
  FunctionNameTest(kAnnoRect, kTestRect);
  valid_indices.push_back(GetInternal()->GetSourceCount());
  DBG_DRAW_RECT(kAnnoRect, kTestRect);
  DBG_DRAW_RECT(kAnnoMissing, kMissingRect);
  valid_indices.push_back(GetInternal()->GetSourceCount());
  DBG_DRAW_RECT(kAnnoMatch, kTestRect);

  SetFilter({TestFilter("", "FunctionNameTest")});
  DBG_DRAW_RECT(kAnnoRect, kMissingRect);
  valid_indices.push_back(0);
  FunctionNameTest(kAnnoRect, kTestRect);

  SetFilter({TestFilter("", "TestBody")});
  FunctionNameTest(kAnnoRect, kMissingRect);
  valid_indices.push_back(GetInternal()->GetSourceCount());
  DBG_DRAW_RECT(kAnnoRect, kTestRect);

  SetFilter({TestFilter("", "", "no_file")});
  DBG_DRAW_RECT(kAnnoRect, kMissingRect);

  SetFilter({TestFilter("", "", __FILE__)});
  valid_indices.push_back(GetInternal()->GetSourceCount());
  DBG_DRAW_RECT(kAnnoRect, kTestRect);

  size_t const kNumDrawCalls = GetInternal()->GetRectCallsTailIdx();
  GetFrameData(true);

  EXPECT_EQ(sources_cache_[0].func, "FunctionNameTest");
  EXPECT_EQ(sources_cache_[0].file, __FILE__);
  EXPECT_EQ(sources_cache_[0].anno, kAnnoRect);
  EXPECT_EQ(sources_cache_[1].func, "TestBody");
  EXPECT_EQ(sources_cache_[1].file, __FILE__);
  EXPECT_EQ(sources_cache_[1].anno, kAnnoRect);
  EXPECT_EQ(sources_cache_[2].anno, kAnnoMissing);
  EXPECT_EQ(sources_cache_[3].anno, kAnnoMatch);

  auto check_draw = [](const VizDebuggerInternal::DrawCall& draw_call,
                       const gfx::Rect& rect, int src_idx, int draw_idx) {
    EXPECT_EQ(draw_call.pos,
              gfx::Vector2dF(rect.origin().x(), rect.origin().y()));
    EXPECT_EQ(draw_call.obj_size, rect.size());
    EXPECT_EQ(draw_call.source_index, src_idx);
    EXPECT_EQ(draw_call.draw_index, draw_idx);
  };
  // Makes sure all valid indices are here and have the correct rect.
  for (size_t i = 0; i < kNumDrawCalls; i++) {
    check_draw(draw_rect_calls_cache_[i], kTestRect, valid_indices[i], i);
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
  SetFilter({TestFilter(kTestFlagFunctionAnnoName, "", "", true, false)});
  EXPECT_FALSE(FlagFunctionTestEnable());
  SetFilter({TestFilter(kTestFlagFunctionAnnoName, "", "", true, true)});
  EXPECT_TRUE(FlagFunctionTestEnable());
  SetFilter({TestFilter(kTestFlagFunctionAnnoName, "", "", true, false)});
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
  SetFilter({TestFilter(kStrA)});
  DBG_DRAW_TEXT(kStrA, gfx::Point(), get_a_string());
  DBG_DRAW_TEXT(kStrB, gfx::Point(), get_b_string());
  EXPECT_EQ(1, count_a);
  EXPECT_EQ(0, count_b);

  // Filter on "anno_B" which should call 'get_b_string'.
  SetFilter({TestFilter(kStrB)});
  DBG_DRAW_TEXT(kStrA, gfx::Point(), get_a_string());
  DBG_DRAW_TEXT(kStrB, gfx::Point(), get_b_string());
  EXPECT_EQ(1, count_a);
  EXPECT_EQ(1, count_b);
}

// This tests passing a single buffer synchronously into the visual debuggeer
TEST_F(VisualDebuggerTest, SingleBufferSync) {
  const char kAnnoRect[] = "annorect";
  const gfx::Rect kTestRect = gfx::Rect(12, 34, 56, 78);
  static const int kNumFrames = 1;
  GetInternal()->ForceEnabled();
  VizDebuggerInternal::BufferInfo buffer_info;
  buffer_info.width = 4;
  buffer_info.height = 4;
  buffer_info.buffer.resize(buffer_info.width * buffer_info.height);
  for (int i = 0; i < buffer_info.height * buffer_info.width; i++) {
    // Random numbers between 0-255 for RGBA values
    uint8_t temp1 = 123;
    uint8_t temp2 = 140;
    uint8_t temp3 = 203;
    uint8_t temp4 = 255;
    buffer_info.buffer[i] = {temp1, temp2, temp3, temp4};
  }
  VizDebuggerInternal::Buffer buffer;
  buffer.id = 0;
  buffer.buffer_info = buffer_info;
  for (uint64_t frame_idx = 0; frame_idx < kNumFrames; frame_idx++) {
    SetFilter({TestFilter({""})});

    static const int kNumSubmission = 1;
    int id = 0;
    DBG_COMPLETE_BUFFERS(id, buffer.buffer_info);
    DBG_DRAW_RECT_BUFF(kAnnoRect, kTestRect, &id);

    GetFrameData(true);

    EXPECT_EQ(counter_, frame_idx);
    EXPECT_EQ(window_x_, 256);
    EXPECT_EQ(window_x_, 256);
    EXPECT_EQ(draw_rect_calls_cache_.size(),
              static_cast<size_t>(kNumSubmission));
    EXPECT_EQ(buffers_.size(), static_cast<size_t>(kNumSubmission));

    if (frame_idx == 0) {
      EXPECT_EQ(sources_cache_.size(), 1u);
      EXPECT_EQ(sources_cache_[0].func, "TestBody");
      EXPECT_EQ(sources_cache_[0].file, __FILE__);
      EXPECT_EQ(sources_cache_[0].anno, kAnnoRect);
    } else {
      // After the first frame there are no new sources in the loop.
      EXPECT_EQ(sources_cache_.size(), 0u);
    }

    EXPECT_EQ(draw_rect_calls_cache_[0].buff_id, 0);

    EXPECT_EQ(buffers_[0].buffer_info.width, buffer.buffer_info.width);
    EXPECT_EQ(buffers_[0].buffer_info.height, buffer.buffer_info.height);
    for (int j = 0;
         j < buffers_[0].buffer_info.width * buffers_[0].buffer_info.height;
         j++) {
      EXPECT_EQ(static_cast<int>(buffers_[0].buffer_info.buffer[j].color_r),
                static_cast<int>(buffer.buffer_info.buffer[j].color_r));
      EXPECT_EQ(static_cast<int>(buffers_[0].buffer_info.buffer[j].color_g),
                static_cast<int>(buffer.buffer_info.buffer[j].color_g));
      EXPECT_EQ(static_cast<int>(buffers_[0].buffer_info.buffer[j].color_b),
                static_cast<int>(buffer.buffer_info.buffer[j].color_b));
      EXPECT_EQ(static_cast<int>(buffers_[0].buffer_info.buffer[j].color_a),
                static_cast<int>(buffer.buffer_info.buffer[j].color_a));
    }
  }
}

// This tests passing multiple buffers into the visual debugger synchronously
TEST_F(VisualDebuggerTest, MultipleBuffersSync) {
  const char kAnnoRect[] = "annorect";
  const gfx::Rect kTestRect = gfx::Rect(12, 34, 56, 78);
  static const int kNumFrames = 1;
  GetInternal()->ForceEnabled();
  GetInternal()->Reset();
  VizDebuggerInternal::BufferInfo buffer_info;
  buffer_info.width = 4;
  buffer_info.height = 4;
  buffer_info.buffer.resize(buffer_info.width * buffer_info.height);
  for (int i = 0; i < buffer_info.height * buffer_info.width; i++) {
    // Random numbers between 0-255 for RGBA values
    uint8_t temp1 = 123;
    uint8_t temp2 = 140;
    uint8_t temp3 = 203;
    uint8_t temp4 = 255;
    buffer_info.buffer[i] = {temp1, temp2, temp3, temp4};
  }
  VizDebuggerInternal::Buffer buffer;
  buffer.id = 0;
  buffer.buffer_info = buffer_info;
  for (uint64_t frame_idx = 0; frame_idx < kNumFrames; frame_idx++) {
    SetFilter({TestFilter("")});

    static const int kNumSubmission = 8;
    for (int i = 0; i < kNumSubmission; i++) {
      int id = i;
      DBG_COMPLETE_BUFFERS(id, buffer.buffer_info);
      DBG_DRAW_RECT_BUFF(kAnnoRect, kTestRect, &id);
    }

    GetFrameData(true);

    EXPECT_EQ(counter_, frame_idx);
    EXPECT_EQ(window_x_, 256);
    EXPECT_EQ(window_x_, 256);
    EXPECT_EQ(draw_rect_calls_cache_.size(),
              static_cast<size_t>(kNumSubmission));
    EXPECT_EQ(buffers_.size(), static_cast<size_t>(kNumSubmission));

    if (frame_idx == 0) {
      EXPECT_EQ(sources_cache_.size(), 1u);
      EXPECT_EQ(sources_cache_[0].func, "TestBody");
      EXPECT_EQ(sources_cache_[0].file, __FILE__);
      EXPECT_EQ(sources_cache_[0].anno, kAnnoRect);
    } else {
      // After the first frame there are no new sources in the loop.
      EXPECT_EQ(sources_cache_.size(), 0u);
    }

    for (int i = 0; i < kNumSubmission; i++) {
      EXPECT_EQ(draw_rect_calls_cache_[i].buff_id, i);

      EXPECT_EQ(buffers_[i].buffer_info.width, buffer.buffer_info.width);
      EXPECT_EQ(buffers_[i].buffer_info.height, buffer.buffer_info.height);
      for (int j = 0;
           j < buffers_[i].buffer_info.width * buffers_[i].buffer_info.height;
           j++) {
        EXPECT_EQ(static_cast<int>(buffers_[i].buffer_info.buffer[j].color_r),
                  static_cast<int>(buffer.buffer_info.buffer[j].color_r));
        EXPECT_EQ(static_cast<int>(buffers_[i].buffer_info.buffer[j].color_g),
                  static_cast<int>(buffer.buffer_info.buffer[j].color_g));
        EXPECT_EQ(static_cast<int>(buffers_[i].buffer_info.buffer[j].color_b),
                  static_cast<int>(buffer.buffer_info.buffer[j].color_b));
        EXPECT_EQ(static_cast<int>(buffers_[i].buffer_info.buffer[j].color_a),
                  static_cast<int>(buffer.buffer_info.buffer[j].color_a));
      }
    }
  }
}

// This tests passing a single buffer into the visual debugger asynchronously
TEST_F(VisualDebuggerTest, SingleBufferAsync) {
  const char kAnnoRect[] = "annorect";
  const gfx::Rect kTestRect = gfx::Rect(12, 34, 56, 78);
  static const int kNumFrames = 2;
  GetInternal()->ForceEnabled();
  GetInternal()->Reset();
  VizDebuggerInternal::BufferInfo buffer_info;
  buffer_info.width = 4;
  buffer_info.height = 4;
  buffer_info.buffer.resize(buffer_info.width * buffer_info.height);
  for (int i = 0; i < buffer_info.height * buffer_info.width; i++) {
    // Random numbers between 0-255 for RGBA values
    uint8_t temp1 = 123;
    uint8_t temp2 = 140;
    uint8_t temp3 = 203;
    uint8_t temp4 = 255;
    buffer_info.buffer[i] = {temp1, temp2, temp3, temp4};
  }
  VizDebuggerInternal::Buffer buffer;
  buffer.id = 0;
  buffer.buffer_info = buffer_info;
  for (uint64_t frame_idx = 0; frame_idx < kNumFrames; frame_idx++) {
    SetFilter({TestFilter("")});

    static const int kNumSubmission = 1;
    static std::vector<VizDebuggerInternal::Buffer> previous_textures;
    for (auto&& each : previous_textures) {
      DBG_COMPLETE_BUFFERS(each.id, each.buffer_info);
    }

    int id = 0;
    DBG_DRAW_RECT_BUFF(kAnnoRect, kTestRect, &id);
    buffer.id = id;
    previous_textures.emplace_back(buffer);

    GetFrameData(true);

    EXPECT_EQ(counter_, frame_idx);
    EXPECT_EQ(window_x_, 256);
    EXPECT_EQ(window_x_, 256);
    EXPECT_EQ(draw_rect_calls_cache_.size(),
              static_cast<size_t>(kNumSubmission));

    if (frame_idx == 0) {
      EXPECT_EQ(sources_cache_.size(), 1u);
      EXPECT_EQ(sources_cache_[0].func, "TestBody");
      EXPECT_EQ(sources_cache_[0].file, __FILE__);
      EXPECT_EQ(sources_cache_[0].anno, kAnnoRect);
      EXPECT_EQ(buffers_.size(), static_cast<size_t>(0));
    } else {
      // After the first frame there are no new sources in the loop.
      EXPECT_EQ(sources_cache_.size(), 0u);
      EXPECT_EQ(buffers_.size(), static_cast<size_t>(kNumSubmission));
      EXPECT_EQ(draw_rect_calls_cache_[0].buff_id, 1);
      EXPECT_EQ(buffers_[0].buffer_info.width, buffer.buffer_info.width);
      EXPECT_EQ(buffers_[0].buffer_info.height, buffer.buffer_info.height);
      for (int j = 0;
           j < buffers_[0].buffer_info.width * buffers_[0].buffer_info.height;
           j++) {
        EXPECT_EQ(static_cast<int>(buffers_[0].buffer_info.buffer[j].color_r),
                  static_cast<int>(buffer.buffer_info.buffer[j].color_r));
        EXPECT_EQ(static_cast<int>(buffers_[0].buffer_info.buffer[j].color_g),
                  static_cast<int>(buffer.buffer_info.buffer[j].color_g));
        EXPECT_EQ(static_cast<int>(buffers_[0].buffer_info.buffer[j].color_b),
                  static_cast<int>(buffer.buffer_info.buffer[j].color_b));
        EXPECT_EQ(static_cast<int>(buffers_[0].buffer_info.buffer[j].color_a),
                  static_cast<int>(buffer.buffer_info.buffer[j].color_a));
      }
    }
  }
}

// This tests passing multiple buffers into the visual debugger asynchronously
TEST_F(VisualDebuggerTest, MultipleBuffersAsync) {
  const char kAnnoRect[] = "annorect";
  const gfx::Rect kTestRect = gfx::Rect(12, 34, 56, 78);
  static const int kNumFrames = 2;
  GetInternal()->ForceEnabled();
  GetInternal()->Reset();
  VizDebuggerInternal::BufferInfo buffer_info;
  buffer_info.width = 4;
  buffer_info.height = 4;
  buffer_info.buffer.resize(buffer_info.width * buffer_info.height);
  VizDebuggerInternal::Buffer buffer;
  buffer.id = 0;
  for (uint64_t frame_idx = 0; frame_idx < kNumFrames; frame_idx++) {
    SetFilter({TestFilter({""})});

    static const int kNumSubmission = 8;
    static std::vector<VizDebuggerInternal::Buffer> previous_textures;
    static std::vector<VizDebuggerInternal::Buffer> test_buffers;
    for (auto&& each : previous_textures) {
      for (int i = 0; i < buffer_info.width * buffer_info.height; i++) {
        // Random numbers between 0-255 for RGBA values
        uint8_t temp1 = (each.id + 15) * 11231;
        uint8_t temp2 = (each.id + 24) * 32461231;
        uint8_t temp3 = (each.id + 523) * 72321231;
        uint8_t temp4 = (each.id + 52) * 321231;
        buffer_info.buffer[i] = {temp1, temp2, temp3, temp4};
      }
      buffer.id = each.id;
      buffer.buffer_info = buffer_info;
      test_buffers.emplace(test_buffers.begin(), buffer);
      DBG_COMPLETE_BUFFERS(buffer.id, buffer.buffer_info);
    }
    previous_textures.resize(kNumSubmission);
    previous_textures.clear();
    for (int i = 0; i < kNumSubmission; i++) {
      int id = i;
      buffer.id = id;
      DBG_DRAW_RECT_BUFF(kAnnoRect, kTestRect, &id);
      buffer.buffer_info = buffer_info;
      previous_textures.emplace(previous_textures.end() - i, buffer);
    }

    GetFrameData(true);

    EXPECT_EQ(counter_, frame_idx);
    EXPECT_EQ(window_x_, 256);
    EXPECT_EQ(window_x_, 256);
    EXPECT_EQ(draw_rect_calls_cache_.size(),
              static_cast<size_t>(kNumSubmission));

    if (frame_idx == 0) {
      EXPECT_EQ(sources_cache_.size(), 1u);
      EXPECT_EQ(sources_cache_[0].func, "TestBody");
      EXPECT_EQ(sources_cache_[0].file, __FILE__);
      EXPECT_EQ(sources_cache_[0].anno, kAnnoRect);
      EXPECT_EQ(buffers_.size(), static_cast<size_t>(0));
    } else {
      // After the first frame there are no new sources in the loop
      EXPECT_EQ(sources_cache_.size(), 0u);
      EXPECT_EQ(buffers_.size(), static_cast<size_t>(kNumSubmission));
      for (int i = 0; i < kNumSubmission; i++) {
        EXPECT_EQ(draw_rect_calls_cache_[i].buff_id, i + 8);
        EXPECT_EQ(buffers_[i].buffer_info.width,
                  test_buffers[i].buffer_info.width);
        EXPECT_EQ(buffers_[i].buffer_info.height,
                  test_buffers[i].buffer_info.height);
        for (int j = 0;
             j < buffers_[i].buffer_info.width * buffers_[i].buffer_info.height;
             j++) {
          EXPECT_EQ(
              static_cast<int>(buffers_[i].buffer_info.buffer[j].color_r),
              static_cast<int>(test_buffers[i].buffer_info.buffer[j].color_r));
          EXPECT_EQ(
              static_cast<int>(buffers_[i].buffer_info.buffer[j].color_g),
              static_cast<int>(test_buffers[i].buffer_info.buffer[j].color_g));
          EXPECT_EQ(
              static_cast<int>(buffers_[i].buffer_info.buffer[j].color_b),
              static_cast<int>(test_buffers[i].buffer_info.buffer[j].color_b));
          EXPECT_EQ(
              static_cast<int>(buffers_[i].buffer_info.buffer[j].color_a),
              static_cast<int>(test_buffers[i].buffer_info.buffer[j].color_a));
        }
      }
    }
  }
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
