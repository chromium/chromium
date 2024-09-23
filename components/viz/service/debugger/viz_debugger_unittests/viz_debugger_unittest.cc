// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <cstdio>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/debugger/viz_debugger_unittests/viz_debugger_internal.h"
#include "components/viz/service/debugger/viz_debugger_unittests/viz_debugger_unittest_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkColor.h"
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
  const gfx::RectF kTestRect = gfx::RectF(12, 34, 56, 78);
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
    EXPECT_EQ(static_cast<int>(draw_calls_cache_.size()), kNumSubmission * 2);
    EXPECT_EQ(static_cast<int>(log_calls_cache_.size()), kNumSubmission);

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
      EXPECT_EQ(draw_calls_cache_[i * 2].uv, kTestUV);
      EXPECT_EQ(draw_calls_cache_[i * 2].pos,
                gfx::Vector2dF(kTestRect.origin().x(), kTestRect.origin().y()));
      EXPECT_EQ(draw_calls_cache_[i * 2].obj_size, kTestRect.size());
      EXPECT_EQ(draw_calls_cache_[i * 2].source_index, 0);
      EXPECT_EQ(draw_calls_cache_[i * 2].draw_index, i * 3);

      EXPECT_EQ(draw_calls_cache_[i * 2 + 1].pos,
                gfx::Vector2dF(kTestRect.origin().x(), kTestRect.origin().y()));
      EXPECT_EQ(draw_calls_cache_[i * 2 + 1].source_index, 1);
      EXPECT_EQ(draw_calls_cache_[i * 2 + 1].draw_index, i * 3 + 1);
      EXPECT_EQ(draw_calls_cache_[i * 2 + 1].text,
                base::StringPrintf("Text %d", i));

      EXPECT_EQ(log_calls_cache_[i].value, base::StringPrintf("%d", i));
      EXPECT_EQ(log_calls_cache_[i].source_index, 2);
      EXPECT_EQ(log_calls_cache_[i].draw_index, i * 3 + 2);
    }
  }
}

static void FunctionNameTest(const char* anno_rect, gfx::RectF rect) {
  DBG_DRAW_RECT(anno_rect, rect);
}

TEST_F(VisualDebuggerTest, FilterDrawSubmission) {
  const char kAnnoRect[] = "annorect";
  const char kAnnoMissing[] = "testmissing";
  const char kAnnoMatch[] = "before_annorect_after";

  GetInternal()->ForceEnabled();
  const gfx::RectF kTestRect = gfx::RectF(10, 30, 50, 70);
  const gfx::RectF kMissingRect = gfx::RectF(11, 33, 55, 77);
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
                       const gfx::RectF& rect, int src_idx, int draw_idx) {
    EXPECT_EQ(draw_call.pos,
              gfx::Vector2dF(rect.origin().x(), rect.origin().y()));
    EXPECT_EQ(draw_call.obj_size, rect.size());
    EXPECT_EQ(draw_call.source_index, src_idx);
    EXPECT_EQ(draw_call.draw_index, draw_idx);
  };
  // Makes sure all valid indices are here and have the correct rect.
  for (size_t i = 0; i < kNumDrawCalls; i++) {
    check_draw(draw_calls_cache_[i], kTestRect, valid_indices[i], i);
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
  DBG_LOG(kStrA, "%s", get_a_string().c_str());
  DBG_LOG(kStrB, "%s", get_b_string().c_str());
  EXPECT_EQ(2, count_a);
  EXPECT_EQ(0, count_b);

  // Filter on "anno_B" which should call 'get_b_string'.
  SetFilter({TestFilter(kStrB)});
  DBG_DRAW_TEXT(kStrA, gfx::Point(), get_a_string());
  DBG_DRAW_TEXT(kStrB, gfx::Point(), get_b_string());
  DBG_LOG(kStrA, "%s", get_a_string().c_str());
  DBG_LOG(kStrB, "%s", get_b_string().c_str());
  EXPECT_EQ(2, count_a);
  EXPECT_EQ(2, count_b);
}

// This tests passing a single buffer synchronously into the visual debuggeer
TEST_F(VisualDebuggerTest, SingleBufferSync) {
  const char kAnnoRect[] = "annorect";
  const gfx::Rect kTestRect = gfx::Rect(12, 34, 56, 78);
  static const int kNumFrames = 1;
  GetInternal()->ForceEnabled();
  VizDebuggerInternal::BufferInfo buffer_info;
  const int kBufferWidth = 4;
  const int kBufferHeight = 8;
  buffer_info.bitmap.setInfo(
      SkImageInfo::MakeN32(kBufferWidth, kBufferHeight, kUnpremul_SkAlphaType));
  buffer_info.bitmap.allocPixels();
  const auto kFillColor = SkColorSetARGB(0xFF, 0x43, 0x67, 0xAA);
  buffer_info.bitmap.eraseColor(kFillColor);
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
    EXPECT_EQ(draw_calls_cache_.size(), static_cast<size_t>(kNumSubmission));
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

    EXPECT_EQ(draw_calls_cache_[0].buff_id, 0);

    auto& pixmap = buffers_[0].buffer_info.bitmap.pixmap();
    EXPECT_EQ(pixmap.info().width(), kBufferWidth);
    EXPECT_EQ(pixmap.info().height(), kBufferHeight);
    for (int j = 0; j < pixmap.height(); j++) {
      for (int i = 0; i < pixmap.width(); i++) {
        EXPECT_EQ(kFillColor, pixmap.getColor(i, j));
      }
    }
  }
}

// This tests passing multiple buffers into the visual debugger synchronously
TEST_F(VisualDebuggerTest, MultipleBuffersSync) {
  const char kAnnoRect[] = "annorect";
  const gfx::Rect kTestRect = gfx::Rect(12, 34, 56, 78);
  static const int kNumFrames = 1;
  GetInternal()->ForceEnabled();
  VizDebuggerInternal::BufferInfo buffer_info;
  const int kBufferWidth = 4;
  const int kBufferHeight = 8;
  buffer_info.bitmap.setInfo(
      SkImageInfo::MakeN32(kBufferWidth, kBufferHeight, kUnpremul_SkAlphaType));
  buffer_info.bitmap.allocPixels();
  const auto kFillColor = SkColorSetARGB(0xFF, 0x43, 0x67, 0xAA);
  buffer_info.bitmap.eraseColor(kFillColor);
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
    EXPECT_EQ(draw_calls_cache_.size(), static_cast<size_t>(kNumSubmission));
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
    auto& pixmap = buffers_[0].buffer_info.bitmap.pixmap();
    EXPECT_EQ(pixmap.info().width(), kBufferWidth);
    EXPECT_EQ(pixmap.info().height(), kBufferHeight);
    for (int j = 0; j < pixmap.height(); j++) {
      for (int i = 0; i < pixmap.width(); i++) {
        EXPECT_EQ(kFillColor, pixmap.getColor(i, j));
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
  VizDebuggerInternal::BufferInfo buffer_info;
  const int kBufferWidth = 4;
  const int kBufferHeight = 8;
  buffer_info.bitmap.setInfo(
      SkImageInfo::MakeN32(kBufferWidth, kBufferHeight, kUnpremul_SkAlphaType));
  buffer_info.bitmap.allocPixels();
  const auto kFillColor = SkColorSetARGB(0xFF, 0x43, 0x67, 0xAA);
  buffer_info.bitmap.eraseColor(kFillColor);
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
    EXPECT_EQ(draw_calls_cache_.size(), static_cast<size_t>(kNumSubmission));

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
      EXPECT_EQ(draw_calls_cache_[0].buff_id, 1);
      auto& pixmap = buffers_[0].buffer_info.bitmap.pixmap();
      EXPECT_EQ(pixmap.info().width(), kBufferWidth);
      EXPECT_EQ(pixmap.info().height(), kBufferHeight);
      for (int j = 0; j < pixmap.height(); j++) {
        for (int i = 0; i < pixmap.width(); i++) {
          EXPECT_EQ(kFillColor, pixmap.getColor(i, j));
        }
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
  const int kBufferWidth = 4;
  const int kBufferHeight = 8;

  std::map<int, SkColor> test_buffers_color;
  for (uint64_t frame_idx = 0; frame_idx < kNumFrames; frame_idx++) {
    SetFilter({TestFilter({""})});

    static const int kNumSubmission = 8;

    for (auto&& each : test_buffers_color) {
      VizDebuggerInternal::BufferInfo buffer_info;
      buffer_info.bitmap.setInfo(SkImageInfo::MakeN32(
          kBufferWidth, kBufferHeight, kUnpremul_SkAlphaType));
      buffer_info.bitmap.allocPixels();
      buffer_info.bitmap.eraseColor(each.second);
      DBG_COMPLETE_BUFFERS(each.first, buffer_info);
    }

    for (int i = 0; i < kNumSubmission; i++) {
      int id = i;
      DBG_DRAW_RECT_BUFF(kAnnoRect, kTestRect, &id);
      // Random numbers between 0-255 for BGRA values
      uint8_t temp[4];
      base::RandBytes(temp);
      const auto kFillColor =
          SkColorSetARGB(temp[0], temp[1], temp[2], temp[3]);
      test_buffers_color[id] = kFillColor;
    }

    GetFrameData(true);

    EXPECT_EQ(counter_, frame_idx);
    EXPECT_EQ(window_x_, 256);
    EXPECT_EQ(window_x_, 256);
    EXPECT_EQ(draw_calls_cache_.size(), static_cast<size_t>(kNumSubmission));

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
        EXPECT_EQ(draw_calls_cache_[i].buff_id, i + 8);
        auto& pixmap = buffers_[i].buffer_info.bitmap.pixmap();

        EXPECT_EQ(pixmap.info().width(), kBufferWidth);
        EXPECT_EQ(pixmap.info().height(), kBufferHeight);
        for (int jj = 0; jj < pixmap.height(); jj++) {
          for (int ii = 0; ii < pixmap.width(); ii++) {
            EXPECT_EQ(test_buffers_color[buffers_[i].id],
                      pixmap.getColor(ii, jj));
          }
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

// This tests makes sure that expressions inside debugging macros are not
// evaluated by default with visual debugger off.
TEST_F(VisualDebuggerTest, DefaultDisabledNoExecute) {
  const char* kStrA = "anno_A";
  // These integers are mutated on a function invocation.
  int count_a = 0;

  auto get_a_string = [&count_a, &kStrA]() {
    count_a++;
    return std::string(kStrA);
  };

  DBG_DRAW_TEXT(kStrA, gfx::Point(), get_a_string());
  DBG_LOG(kStrA, "%s", get_a_string().c_str());
  EXPECT_EQ(0, count_a);

  EXPECT_EQ(get_a_string(), kStrA);
  EXPECT_EQ(1, count_a);
}

// For optimization purposes the flag fbool values return false as a constexpr.
// This allows the compiler to constant propagate and remove unused codepaths.
static_assert(flag_default_value_check() == false,
              "Default value when debugger is disabled is false.");

#endif
