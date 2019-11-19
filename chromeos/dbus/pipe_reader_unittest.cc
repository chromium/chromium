// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/pipe_reader.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

void CopyResult(base::RunLoop* run_loop,
                base::Optional<std::string>* output,
                base::Optional<std::string> result) {
  run_loop->Quit();
  *output = std::move(result);
}

// Writes the |data| to |fd|, then close |fd|.
void WriteData(base::ScopedFD fd, const std::string& data) {
  EXPECT_TRUE(base::WriteFileDescriptor(fd.get(), data.data(), data.size()));
}

}  // namespace

class PipeReaderTest : public testing::Test {
 public:
  PipeReaderTest() = default;
  ~PipeReaderTest() override {
    // Flush the TaskEnvironment to prevent leaks of PostTaskAndReply
    // callbacks.
    task_environment_.RunUntilIdle();
  }

  scoped_refptr<base::TaskRunner> GetTaskRunner() {
    return task_environment_.GetMainThreadTaskRunner();
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PipeReaderTest, Empty) {
  auto reader = std::make_unique<PipeReader>(GetTaskRunner());
  base::RunLoop run_loop;
  base::Optional<std::string> output;
  base::ScopedFD write_fd =
      reader->StartIO(base::BindOnce(&CopyResult, &run_loop, &output));
  write_fd.reset();
  run_loop.Run();
  EXPECT_EQ(std::string(), output);
}

TEST_F(PipeReaderTest, SmallData) {
  constexpr char kSmallData[] = "abcdefghijklmnopqrstuvwxyz";

  auto reader = std::make_unique<PipeReader>(GetTaskRunner());
  base::RunLoop run_loop;
  base::Optional<std::string> output;
  base::ScopedFD write_fd =
      reader->StartIO(base::BindOnce(&CopyResult, &run_loop, &output));
  base::PostTask(FROM_HERE,
                 base::BindOnce(&WriteData, std::move(write_fd), kSmallData));
  run_loop.Run();
  EXPECT_EQ(std::string(kSmallData), output);
}

TEST_F(PipeReaderTest, LargeData) {
  // Larger than internal buffer size (=4096 bytes).
  const std::string large_data(8192, 'a');

  auto reader = std::make_unique<PipeReader>(GetTaskRunner());
  base::RunLoop run_loop;
  base::Optional<std::string> output;
  base::ScopedFD write_fd =
      reader->StartIO(base::BindOnce(&CopyResult, &run_loop, &output));
  base::PostTask(FROM_HERE,
                 base::BindOnce(&WriteData, std::move(write_fd), large_data));
  run_loop.Run();
  EXPECT_EQ(large_data, output);
}

TEST_F(PipeReaderTest, Cancel) {
  auto reader = std::make_unique<PipeReader>(GetTaskRunner());
  base::ScopedFD write_fd =
      reader->StartIO(base::BindOnce([](base::Optional<std::string> result) {
        FAIL();  // Unexpected to be called.
      }));
  reader.reset();  // Delete |reader| before closing |write_fd|.
}

}  // namespace chromeos
