// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/service_worker/service_worker_storage_test_utils.h"

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

namespace {

void ReadDataPipeInternal(mojo::DataPipeConsumerHandle handle,
                          std::string* result,
                          base::OnceClosure quit_closure) {
  while (true) {
    base::span<const uint8_t> buffer;
    MojoResult rv = handle.BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
    switch (rv) {
      case MOJO_RESULT_BUSY:
      case MOJO_RESULT_INVALID_ARGUMENT:
        NOTREACHED_IN_MIGRATION();
        return;
      case MOJO_RESULT_FAILED_PRECONDITION:
        std::move(quit_closure).Run();
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&ReadDataPipeInternal, handle, result,
                                      std::move(quit_closure)));
        return;
      case MOJO_RESULT_OK:
        ASSERT_NE(buffer.data(), nullptr);
        ASSERT_GT(buffer.size(), 0u);
        size_t before_size = result->size();
        result->append(base::as_string_view(buffer));
        size_t read_size = result->size() - before_size;
        EXPECT_EQ(buffer.size(), read_size);
        rv = handle.EndReadData(read_size);
        EXPECT_EQ(rv, MOJO_RESULT_OK);
        break;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return;
}

}  // namespace

namespace test {

std::string ReadDataPipeViaRunLoop(mojo::ScopedDataPipeConsumerHandle handle) {
  EXPECT_TRUE(handle.is_valid());
  std::string result;
  base::RunLoop loop;
  ReadDataPipeInternal(handle.get(), &result, loop.QuitClosure());
  loop.Run();
  return result;
}

}  // namespace test
}  // namespace storage
