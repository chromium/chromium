// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/background_file.h"

#include <cstddef>
#include <memory>

#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "components/language_detection/testing/language_detection_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language_detection {

// `SequenceChecker` does not expose a type that you can reference (because
// the type varies at compilation). So this class wraps that to make it possible
// to pass one around.
struct SequenceCheckerWrapper {
  SEQUENCE_CHECKER(sequence_checker);
};

class BackgroundFileTest : public ::testing::Test {
 public:
  BackgroundFileTest()
      : background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {}

  scoped_refptr<base::SequencedTaskRunner> background_task_runner() {
    return background_task_runner_;
  }

  // Initializes a `SequenceChecker` for the background thread. Waits until
  // complete.
  void InitBackgroundSequenceChecker() {
    // Create the sequence checker on the background thread and then assign it
    // on the main thread.
    background_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce([]() {
          return std::make_unique<SequenceCheckerWrapper>();
        }),
        base::BindOnce(
            [](std::unique_ptr<SequenceCheckerWrapper>* sequence_checker_ptr,
               std::unique_ptr<SequenceCheckerWrapper> sequence_checker) {
              *sequence_checker_ptr = std::move(sequence_checker);
            },
            base::Unretained(&background_sequence_checker_)));
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return !!background_sequence_checker_; }));
  }

  // `DCHECK`s that we are currently on the main sequence.
  void DcheckOnMainSequence() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_.sequence_checker);
  }

  // `DCHECK`s that we are currently on the background sequence.
  void DcheckOnBackgroundSequence() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        background_sequence_checker_->sequence_checker);
  }

  // Replace the file in `backgroud_file` with a valid or invalid file depending
  // on the value of `valid`. Returns when the operation has completed.
  void ReplaceFile(BackgroundFile& background_file, bool valid) {
    bool replaced_callback_was_called = false;
    background_file.ReplaceFile(
        /*file_opener=*/base::BindOnce(
            [](BackgroundFileTest* test, bool valid) {
              test->DcheckOnBackgroundSequence();
              return valid ? GetValidModelFile() : base::File();
            },
            base::Unretained(this), valid),
        /*replaced_callback=*/base::BindOnce(
            [](BackgroundFileTest* test,
               bool* replaced_callback_was_called_ptr) {
              test->DcheckOnMainSequence();
              *replaced_callback_was_called_ptr = true;
            },
            base::Unretained(this),
            base::Unretained(&replaced_callback_was_called)));
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return replaced_callback_was_called; }));
  }

 private:
  // Needed for a test with task runners.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::SYSTEM_TIME};

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  SequenceCheckerWrapper main_sequence_checker_;
  std::unique_ptr<SequenceCheckerWrapper> background_sequence_checker_;

  // There should be no blocking on the main thread during any of these tests
  // except to wait for things to happen.
  base::ScopedDisallowBlocking disallow_blocking;
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync;
};

TEST_F(BackgroundFileTest, StartsInvalid) {
  BackgroundFile file(background_task_runner());
  ASSERT_FALSE(file.GetFile().IsValid());
}

// Tests that replacing the file with an valid or invalid file results in a
// the expected file being available and everything happens on the correct
// sequences.
TEST_F(BackgroundFileTest, ReplaceFile) {
  InitBackgroundSequenceChecker();
  BackgroundFile file(background_task_runner());
  ReplaceFile(file, /*valid*/ true);
  ASSERT_TRUE(file.GetFile().IsValid());
  ReplaceFile(file, /*valid*/ false);
  ASSERT_FALSE(file.GetFile().IsValid());
}

// Test that `InvalidateFile` does so immediately.
TEST_F(BackgroundFileTest, InvalidateFileIsImmediate) {
  InitBackgroundSequenceChecker();
  BackgroundFile file(background_task_runner());
  ReplaceFile(file, /*valid*/ true);
  ASSERT_TRUE(file.GetFile().IsValid());
  // No need to wait, the file should be immediately invalid even though the
  // previous file is being closed in the background.
  file.InvalidateFile();
  ASSERT_FALSE(file.GetFile().IsValid());
}

// Test that if the `BackgroundFile` is destroyed mid-replace, that the
// `base::File` returned by the file opener is closed off the main thread.
TEST_F(BackgroundFileTest, DestroyWhileReplacing) {
  InitBackgroundSequenceChecker();
  bool replaced_callback_was_called = false;
  {
    BackgroundFile file(background_task_runner());
    file.ReplaceFile(
        /*file_opener=*/base::BindOnce([]() {
          base::File file = GetValidModelFile();
          return file;
        }),
        /*replaced_callback=*/base::BindOnce(
            [](bool* replaced_callback_was_called_ptr) {
              *replaced_callback_was_called_ptr = true;
            },
            base::Unretained(&replaced_callback_was_called)));
  }

  // Since the replaced callback should not be called, we cannot wait for that.
  // Instead we post a do-nothing task to the background task runner. This will
  // be blocked by the file loading and we wait for the reply.
  bool queue_flushed = false;
  background_task_runner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce([]() {}),
      base::BindOnce([](bool* queue_flushed_ptr) { *queue_flushed_ptr = true; },
                     base::Unretained(&queue_flushed)));
  ASSERT_TRUE(base::test::RunUntil([&]() { return queue_flushed; }));
  ASSERT_FALSE(replaced_callback_was_called);
}
}  // namespace language_detection
