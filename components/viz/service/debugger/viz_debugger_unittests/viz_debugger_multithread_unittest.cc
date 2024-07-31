// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/debugger/viz_debugger_unittests/viz_debugger_internal.h"
#include "components/viz/service/debugger/viz_debugger_unittests/viz_debugger_unittest_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/vector2d_f.h"

#if VIZ_DEBUGGER_IS_ON()
using testing::_;
using testing::Mock;

namespace viz {

namespace {

class VizDebuggerMultithreadTest : public VisualDebuggerTestBase {};

static_assert(sizeof(VizDebuggerInternal) == sizeof(VizDebugger),
              "This test code exposes the internals of |VizDebugger| via an "
              "upcast; thus they must be the same size.");

enum class VizDebugCommandType { kDbgDrawRectCommand, kDbgLogCommand };

// Data structure to hold pre-determined commands for unit test.
struct DbgTestConfig {
  std::vector<std::vector<VizDebugCommandType>> dbg_commands;
};

struct DbgCommandIdentifier {
  int dbg_command_thread_id;
  int dbg_command_id;
  int dbg_command_frame_id;
};

// Reader thread.
class ReaderTestThread : public base::PlatformThread::Delegate {
 public:
  ReaderTestThread() = default;

  ReaderTestThread(const ReaderTestThread&) = delete;
  ReaderTestThread& operator=(const ReaderTestThread&) = delete;

  // Index variables to keep track of which debug command calls are being
  // checked in each called debug command cache.
  int rect_command_idx = 0;
  int log_command_idx = 0;

  // Identification pairs consisting of debug command ID and type of command.
  std::vector<DbgCommandIdentifier> draw_rect_command_history;
  std::vector<DbgCommandIdentifier> log_command_history;

  // Initialize debug commands before threads run.
  void Init(DbgTestConfig& config, uint32_t num_frames, uint32_t spin_amount) {
    thread_test_config_ = config;
    num_frames_ = num_frames;
    spin_amount_ = spin_amount;
  }

  void ThreadMain() override {
    // NOTE: Thread ID's are not unique on Fuchsia. Also, thread ID's on
    // Fuchsia are very large and thread_id_ may not hold the true thread ID
    // value. Regardless, the unit tests will run successfully.
    thread_id_ = base::PlatformThread::CurrentId();
    for (int i = 0;
         static_cast<uint32_t>(i) < thread_test_config_.dbg_commands[0].size();
         ++i) {
      // Spin to allow writer to jump in once in a while.
      for (uint32_t spinner = 0; spinner < spin_amount_; ++spinner) {
        ++spin_inc_var_;
      }
      // The position of the rect is used as a unique identifier
      // for draw rect and text command calls for cross-checking later.
      // The x-position represents the thread ID and the
      // y-position represents the debug command ID per thread.
      // gfx::Rect(int x, int y, int width, int height)
      const gfx::RectF testRect = gfx::RectF(thread_id_, i, 12, 34);
      if (thread_test_config_.dbg_commands[0][i] ==
          VizDebugCommandType::kDbgDrawRectCommand) {
        DBG_DRAW_RECTANGLE_OPT_BUFF_UV_TEXT(
            "multithread rect", DBG_OPT_BLACK, testRect.OffsetFromOrigin(),
            testRect.size(), nullptr, DBG_DEFAULT_UV, "text");
        draw_rect_command_history.push_back({static_cast<int>(thread_id_), i});
      } else {  // kDbgLogCommand case
        DBG_LOG("multithread log", "%d:%d", thread_id_, i);
        log_command_history.push_back({static_cast<int>(thread_id_), i});
      }
    }
  }

 private:
  DbgTestConfig thread_test_config_;
  uint32_t thread_id_;
  uint32_t num_frames_;
  uint32_t spin_amount_;
  std::atomic<int> spin_inc_var_ = 0;
};

// Writer thread.
class WriterTestThread : public base::PlatformThread::Delegate {
 public:
  WriterTestThread() = default;

  WriterTestThread(const WriterTestThread&) = delete;
  WriterTestThread& operator=(const WriterTestThread&) = delete;

  // Initialize parameters before threads run.
  void Init(VizDebuggerMultithreadTest* test_fixture_ptr,
            uint32_t num_writer_tries,
            uint32_t spin_amount) {
    spin_amount_ = spin_amount;
    test_fixture_ptr_ = test_fixture_ptr;
    num_writer_tries_ = num_writer_tries;
  }

  void ThreadMain() override {
    for (uint32_t writer_try = 0; writer_try < num_writer_tries_;
         ++writer_try) {
      // Spin to add delays before writer tries again.
      for (uint32_t spinner = 0; spinner < spin_amount_; ++spinner) {
        ++spin_inc_var_;
      }
      test_fixture_ptr_->GetFrameData(false);
    }
  }

 private:
  uint32_t spin_amount_;
  std::atomic<int> spin_inc_var_ = 0;
  raw_ptr<VizDebuggerMultithreadTest> test_fixture_ptr_;
  uint32_t num_writer_tries_;
};

// Tests k-number of READ operations from k different threads.
//
// NOTE: The test assumes that thread ID's are unique
// (Exception: Fuchsia thread ID's are not unique).
TEST_F(VizDebuggerMultithreadTest, kReadersTest) {
  static const unsigned kNumReaderThreads = 3;

  ReaderTestThread threads[kNumReaderThreads];
  base::PlatformThreadHandle handles[kNumReaderThreads];

  SetFilter({TestFilter("")});
  // Enable viz debugger
  GetInternal()->ForceEnabled();

  DbgTestConfig test_config;

  static const unsigned kNumDrawRectCommandsPerFrame = 64;
  static const unsigned kNumLogCommandsPerFrame = 64;
  uint32_t kNumCommandsPerFrame =
      kNumDrawRectCommandsPerFrame + kNumLogCommandsPerFrame;

  // pre-process test commands vector
  test_config.dbg_commands.resize(1);
  test_config.dbg_commands[0].reserve(kNumCommandsPerFrame);

  for (uint32_t i = 0; i < kNumDrawRectCommandsPerFrame; ++i) {
    test_config.dbg_commands[0].push_back(
        VizDebugCommandType::kDbgDrawRectCommand);
  }

  for (uint32_t i = 0; i < kNumLogCommandsPerFrame; ++i) {
    test_config.dbg_commands[0].push_back(VizDebugCommandType::kDbgLogCommand);
  }

  // Initialize each thread and start thread
  for (uint32_t i = 0; i < kNumReaderThreads; ++i) {
    threads[i].Init(test_config, 1, 0);
  }
  for (uint32_t i = 0; i < kNumReaderThreads; ++i) {
    ASSERT_TRUE(base::PlatformThread::Create(0, &threads[i], &handles[i]));
  }

  // Collect all threads
  for (auto& handle : handles) {
    base::PlatformThread::Join(handle);
  }

  int expected_submission_count = kNumCommandsPerFrame * kNumReaderThreads;

  EXPECT_EQ(GetInternal()->GetSubmissionCount(), expected_submission_count);

  std::vector<VizDebuggerInternal::DrawCall> rect_calls_result =
      GetInternal()->GetDrawRectCalls();
  std::vector<VizDebuggerInternal::LogCall> logs_result =
      GetInternal()->GetLogs();

  size_t const kNumDrawCallSubmission = static_cast<size_t>(std::min(
      GetInternal()->GetRectCallsTailIdx(), GetInternal()->GetRectCallsSize()));
  size_t const kNumLogSubmission = static_cast<size_t>(
      std::min(GetInternal()->GetLogsTailIdx(), GetInternal()->GetLogsSize()));

  bool valid_command_found;

  // Final cross-checking for draw rect calls.
  for (size_t i = 0; i < kNumDrawCallSubmission; ++i) {
    valid_command_found = false;
    for (auto&& thread : threads) {
      // If debug calls from a thread is exhausted, skip this thread.
      if (static_cast<uint32_t>(thread.rect_command_idx) >=
          thread.draw_rect_command_history.size()) {
        continue;
      }
      DbgCommandIdentifier cur_thread_command_identifier =
          thread.draw_rect_command_history[thread.rect_command_idx];

      // Check if debug command ID matches. Refer to ReaderTestThread's
      // ThreadMain() function to see how command ID's are formatted.
      if (rect_calls_result[i].pos.x() ==
              cur_thread_command_identifier.dbg_command_thread_id &&
          rect_calls_result[i].pos.y() ==
              cur_thread_command_identifier.dbg_command_id) {
        valid_command_found = true;
        ++thread.rect_command_idx;
        break;
      }
    }
    EXPECT_TRUE(valid_command_found);
  }

  // Final cross-checking for logs.
  for (size_t i = 0; i < kNumLogSubmission; ++i) {
    valid_command_found = false;
    for (auto&& thread : threads) {
      // If debug calls from a thread is exhausted, skip this thread.
      if (static_cast<uint32_t>(thread.log_command_idx) >=
          thread.log_command_history.size()) {
        continue;
      }
      DbgCommandIdentifier cur_thread_command_identifier =
          thread.log_command_history[thread.log_command_idx];

      // Extract debug command identifier information from log result.
      // Information is being extracted froma string below. Refer to
      // ReaderTestThread's ThreadMain() function to see how command ID's
      // are formatted.
      std::string log_result_identifier = logs_result[i].value;
      int log_split_position = log_result_identifier.find(":");
      int log_thread_id;
      int log_command_id;
      base::StringToInt(log_result_identifier.substr(0, log_split_position),
                        &log_thread_id);
      base::StringToInt(log_result_identifier.substr(log_split_position + 1),
                        &log_command_id);

      // Check if debug command ID matches.
      if (log_thread_id ==
              cur_thread_command_identifier.dbg_command_thread_id &&
          log_command_id == cur_thread_command_identifier.dbg_command_id) {
        valid_command_found = true;
        ++thread.log_command_idx;
        break;
      }
    }
    EXPECT_TRUE(valid_command_found);
  }
}

// Tests k-number of READ operations from k different reader threads while
// ONE writer thread comes in and writes once in a while.
//
// NOTE: The test assumes that thread ID's are unique
// (Exception: Fuchsia thread ID's are not unique).
TEST_F(VizDebuggerMultithreadTest, kReadersOneWriterCommandsSequenceTest) {
  // Number of Reader Threads
  static const unsigned kNumReaderThreads = 3;
  // Number of times writer thread tries to run
  static const unsigned kNumWriterTries = 20;
  // Number of DBG calls per frame for each type
  static const unsigned kNumDrawRectCommandsPerFrame = 64;
  static const unsigned kNumLogCommandsPerFrame = 64;
  // Amount of spin delay for each type of thread
  static const unsigned kWriterSpinAmount = 900;
  static const unsigned kReaderSpinAmount = 100;

  WriterTestThread writer_thread;
  base::PlatformThreadHandle writer_thread_handle;
  ReaderTestThread reader_threads[kNumReaderThreads];
  base::PlatformThreadHandle reader_thread_handles[kNumReaderThreads];

  SetFilter({TestFilter("")});
  // Enable viz debugger
  GetInternal()->ForceEnabled();

  DbgTestConfig test_config;

  uint32_t kNumCommandsPerFrame =
      kNumDrawRectCommandsPerFrame + kNumLogCommandsPerFrame;

  // Pre-process test commands vector.
  test_config.dbg_commands.resize(1);
  test_config.dbg_commands[0].reserve(kNumCommandsPerFrame);

  // Populate test commands vectors with enums representing command types.
  for (uint32_t i = 0; i < kNumDrawRectCommandsPerFrame; ++i) {
    test_config.dbg_commands[0].push_back(
        VizDebugCommandType::kDbgDrawRectCommand);
  }

  for (uint32_t i = 0; i < kNumLogCommandsPerFrame; ++i) {
    test_config.dbg_commands[0].push_back(VizDebugCommandType::kDbgLogCommand);
  }

  // Initialize and start each thread. Each thread will start making VizDebugger
  // debug calls simultaneously upon starting.
  for (uint32_t i = 0; i < kNumReaderThreads; ++i) {
    reader_threads[i].Init(test_config, 1, kReaderSpinAmount);
  }
  for (uint32_t i = 0; i < kNumReaderThreads; ++i) {
    ASSERT_TRUE(base::PlatformThread::Create(0, &reader_threads[i],
                                             &reader_thread_handles[i]));
  }
  writer_thread.Init(this, kNumWriterTries, kWriterSpinAmount);
  ASSERT_TRUE(
      base::PlatformThread::Create(0, &writer_thread, &writer_thread_handle));

  // Collect all threads.
  base::PlatformThread::Join(writer_thread_handle);
  for (auto& reader_thread_handle : reader_thread_handles) {
    base::PlatformThread::Join(reader_thread_handle);
  }

  // One last call to GetFrameData() to collect any remaining calls not yet
  // added to the debug calls cache.
  GetFrameData(false);
  int expected_submission_count = kNumCommandsPerFrame * kNumReaderThreads;
  int calls_cache_total_size =
      draw_calls_cache_.size() + log_calls_cache_.size();
  EXPECT_EQ(calls_cache_total_size, expected_submission_count);

  bool valid_command_found;

  // Final cross-checking for draw rect calls.
  for (auto&& rect_call : draw_calls_cache_) {
    valid_command_found = false;
    for (auto&& thread : reader_threads) {
      // If debug calls from a thread is exhausted, skip this thread.
      if (static_cast<uint32_t>(thread.rect_command_idx) >=
          thread.draw_rect_command_history.size()) {
        continue;
      }
      DbgCommandIdentifier cur_thread_command_identifier =
          thread.draw_rect_command_history[thread.rect_command_idx];

      // Check if debug command ID matches. Refer to ReaderTestThread's
      // ThreadMain() function to see how command ID's are formatted.
      if (rect_call.pos.x() ==
              cur_thread_command_identifier.dbg_command_thread_id &&
          rect_call.pos.y() == cur_thread_command_identifier.dbg_command_id) {
        valid_command_found = true;
        ++thread.rect_command_idx;
        break;
      }
    }
    EXPECT_TRUE(valid_command_found);
  }

  // Final cross-checking for logs.
  for (auto&& log_call : log_calls_cache_) {
    valid_command_found = false;
    for (auto&& thread : reader_threads) {
      // If debug calls from a thread is exhausted, skip this thread.
      if (static_cast<uint32_t>(thread.log_command_idx) >=
          thread.log_command_history.size()) {
        continue;
      }
      DbgCommandIdentifier cur_thread_command_identifier =
          thread.log_command_history[thread.log_command_idx];

      // Extract debug command identifier information from log result.
      // Information is being extracted from a string below. Refer to
      // ReaderTestThread's ThreadMain() function to see how command ID's
      // are formatted.
      std::string log_result_identifier = log_call.value;
      int log_split_position = log_result_identifier.find(":");
      int log_thread_id;
      int log_command_id;
      base::StringToInt(log_result_identifier.substr(0, log_split_position),
                        &log_thread_id);
      base::StringToInt(log_result_identifier.substr(log_split_position + 1),
                        &log_command_id);

      // Check if debug command ID matches.
      if (log_thread_id ==
              cur_thread_command_identifier.dbg_command_thread_id &&
          log_command_id == cur_thread_command_identifier.dbg_command_id) {
        valid_command_found = true;
        ++thread.log_command_idx;
        break;
      }
    }
    EXPECT_TRUE(valid_command_found);
  }
}
}  // namespace
}  // namespace viz

#endif  // VIZ_DEBUGGER_IS_ON()
