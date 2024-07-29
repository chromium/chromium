// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/check.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/cronet/native/test/test_util.h"
#include "cronet_c.h"
#include "partition_alloc/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BufferTest : public ::testing::Test {
 public:
  BufferTest() = default;

  BufferTest(const BufferTest&) = delete;
  BufferTest& operator=(const BufferTest&) = delete;

  ~BufferTest() override {}

 protected:
  static void BufferCallback_OnDestroy(Cronet_BufferCallbackPtr self,
                                       Cronet_BufferPtr buffer);
  bool on_destroy_called() const { return on_destroy_called_; }

  // Provide a task environment for use by TestExecutor instances. Do not
  // initialize the ThreadPool as this is done by the Cronet_Engine
  base::test::SingleThreadTaskEnvironment task_environment_;

 private:
  void set_on_destroy_called(bool value) { on_destroy_called_ = value; }

  bool on_destroy_called_ = false;
};

const uint64_t kTestBufferSize = 20;

// static
void BufferTest::BufferCallback_OnDestroy(Cronet_BufferCallbackPtr self,
                                          Cronet_BufferPtr buffer) {
  CHECK(self);
  Cronet_ClientContext context = Cronet_BufferCallback_GetClientContext(self);
  BufferTest* test = static_cast<BufferTest*>(context);
  CHECK(test);
  test->set_on_destroy_called(true);
  // Free buffer_data.
  void* buffer_data = Cronet_Buffer_GetData(buffer);
  CHECK(buffer_data);
  free(buffer_data);
}

// Test on_destroy that destroys the buffer set in context.
void TestRunnable_DestroyBuffer(Cronet_RunnablePtr self) {
  CHECK(self);
  Cronet_ClientContext context = Cronet_Runnable_GetClientContext(self);
  Cronet_BufferPtr buffer = static_cast<Cronet_BufferPtr>(context);
  CHECK(buffer);
  // Destroy buffer. TestCronet_BufferCallback_OnDestroy should be invoked.
  Cronet_Buffer_Destroy(buffer);
}

// Example of allocating buffer with reasonable size.
TEST_F(BufferTest, TestInitWithAlloc) {
  // Create Cronet buffer and allocate buffer data.
  Cronet_BufferPtr buffer = Cronet_Buffer_Create();
  Cronet_Buffer_InitWithAlloc(buffer, kTestBufferSize);
  EXPECT_TRUE(Cronet_Buffer_GetData(buffer));
  EXPECT_EQ(Cronet_Buffer_GetSize(buffer), kTestBufferSize);
  Cronet_Buffer_Destroy(buffer);
  ASSERT_FALSE(on_destroy_called());
}

#if defined(ARCH_CPU_64_BITS) &&                                              \
    (defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) ||               \
     defined(THREAD_SANITIZER) ||                                             \
     PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) || BUILDFLAG(IS_CHROMEOS) || \
     BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_FUCHSIA))
// - ASAN and MSAN malloc by default triggers crash instead of returning null on
//   failure.
// - PartitionAlloc malloc also crashes on allocation failure by design.
// - Fuchsia malloc() also crashes on allocation failure in some kernel builds.
// - On Linux and Chrome OS, the allocator shims crash for large allocations, on
//   purpose.
#define MAYBE_TestInitWithHugeAllocFails DISABLED_TestInitWithHugeAllocFails
#else
#define MAYBE_TestInitWithHugeAllocFails TestInitWithHugeAllocFails
#endif
// Verify behaviour when an unsatisfiably huge buffer allocation is requested.
// On 32-bit platforms, we want to ensure that a 64-bit range allocation size
// is rejected, rather than resulting in a 32-bit truncated allocation.
// Some platforms over-commit allocations, so we request an allocation of the
// whole 64-bit address-space, which cannot possibly be satisfied in a 32- or
// 64-bit process.
TEST_F(BufferTest, MAYBE_TestInitWithHugeAllocFails) {
  Cronet_BufferPtr buffer = Cronet_Buffer_Create();
  const uint64_t kHugeTestBufferSize = std::numeric_limits<uint64_t>::max();
  Cronet_Buffer_InitWithAlloc(buffer, kHugeTestBufferSize);
  EXPECT_FALSE(Cronet_Buffer_GetData(buffer));
  EXPECT_EQ(Cronet_Buffer_GetSize(buffer), 0ull);
  Cronet_Buffer_Destroy(buffer);
  ASSERT_FALSE(on_destroy_called());
}

// Example of initializing buffer with app-allocated data.
TEST_F(BufferTest, TestInitWithDataAndCallback) {
  Cronet_BufferCallbackPtr buffer_callback =
      Cronet_BufferCallback_CreateWith(BufferCallback_OnDestroy);
  Cronet_BufferCallback_SetClientContext(buffer_callback, this);
  // Create Cronet buffer and allocate buffer data.
  Cronet_BufferPtr buffer = Cronet_Buffer_Create();
  Cronet_Buffer_InitWithDataAndCallback(buffer, malloc(kTestBufferSize),
                                        kTestBufferSize, buffer_callback);
  EXPECT_TRUE(Cronet_Buffer_GetData(buffer));
  EXPECT_EQ(Cronet_Buffer_GetSize(buffer), kTestBufferSize);
  Cronet_Buffer_Destroy(buffer);
  ASSERT_TRUE(on_destroy_called());
  Cronet_BufferCallback_Destroy(buffer_callback);
}

// Example of posting application on_destroy to the executor and passing
// buffer to it, expecting buffer to be destroyed and freed.
TEST_F(BufferTest, TestCronetBufferAsync) {
  // Executor provided by the application.
  Cronet_ExecutorPtr executor = cronet::test::CreateTestExecutor();
  Cronet_BufferCallbackPtr buffer_callback =
      Cronet_BufferCallback_CreateWith(BufferCallback_OnDestroy);
  Cronet_BufferCallback_SetClientContext(buffer_callback, this);
  // Create Cronet buffer and allocate buffer data.
  Cronet_BufferPtr buffer = Cronet_Buffer_Create();
  Cronet_Buffer_InitWithDataAndCallback(buffer, malloc(kTestBufferSize),
                                        kTestBufferSize, buffer_callback);
  Cronet_RunnablePtr runnable =
      Cronet_Runnable_CreateWith(TestRunnable_DestroyBuffer);
  Cronet_Runnable_SetClientContext(runnable, buffer);
  Cronet_Executor_Execute(executor, runnable);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(on_destroy_called());
  Cronet_Executor_Destroy(executor);
  Cronet_BufferCallback_Destroy(buffer_callback);
}

}  // namespace
