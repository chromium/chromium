// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/memory/userspace_swap/userfaultfd.h"

#include <fcntl.h>
#include <linux/unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>

#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/rand_util.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace memory {
namespace userspace_swap {

namespace {

using testing::_;
using testing::ByRef;
using testing::Eq;
using testing::Exactly;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

// ScopedMemory is a simple RAII memory class around mmap that simplifies
// tests.
class ScopedMemory {
 public:
  ScopedMemory(const ScopedMemory&) = delete;
  ScopedMemory& operator=(const ScopedMemory&) = delete;

  ~ScopedMemory() { Free(); }

  ScopedMemory() = default;
  explicit ScopedMemory(size_t size) { Alloc(size, PROT_READ | PROT_WRITE); }
  ScopedMemory(size_t size, int protections) { Alloc(size, protections); }

  void* Alloc(size_t size, int protections) {
    Free();
    ptr_ = mmap(nullptr, size, protections, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    len_ = size;
    return ptr_;
  }

  void Free() {
    if (is_valid()) {
      munmap(ptr_, len_);
      ptr_ = nullptr;
      len_ = 0;
    }
  }

  void* Remap(size_t new_size) {
    ptr_ = mremap(ptr_, len_, new_size, MREMAP_MAYMOVE, nullptr);
    len_ = new_size;
    return ptr_;
  }

  void* Release() {
    void* ptr = ptr_;
    ptr_ = nullptr;
    return ptr;
  }

  operator bool() { return is_valid(); }

  operator uintptr_t() { return reinterpret_cast<uintptr_t>(ptr_); }

  template <typename T>
  operator T*() {
    return static_cast<T*>(ptr_);
  }

  bool is_valid() const { return ptr_ != nullptr && ptr_ != MAP_FAILED; }
  void* get() { return ptr_; }

 private:
  // RAW_PTR_EXCLUSION: Never allocated by PartitionAlloc (always mmap'ed), so
  // there is no benefit to using a raw_ptr, only cost.
  RAW_PTR_EXCLUSION void* ptr_ = nullptr;
  size_t len_ = 0;
};

const size_t kPageSize = base::GetPageSize();

}  // namespace

class UserfaultFDTest : public testing::Test {
 public:
  void SetUp() override {
    // We skip these tests if the kernel does not support userfaultfd
    // or when we have insufficient permissions.
    if (!UserfaultFD::KernelSupportsUserfaultFD()) {
      GTEST_SKIP() << "Skipping test: no userfaultfd(2) support.";
    }
    if (!CreateUffd() && errno == EPERM) {
      GTEST_SKIP() << "Skipping test: userfaultfd(2) not permitted.";
    }
  }

  void TearDown() override {
    if (uffd_) {
      uffd_->CloseAndStopWaitingForEvents();
    }
  }

 protected:
  bool CreateUffd(
      UserfaultFD::Features features = static_cast<UserfaultFD::Features>(0)) {
    uffd_ = UserfaultFD::Create(features);

    PLOG_IF(ERROR, !uffd_) << "UserfaultFD::Create failed";
    return uffd_ != nullptr;
  }

  int fd() {
    if (!uffd_)
      return -1;

    return uffd_->fd_.get();
  }

  // We need to allow our main test thread to run in parallel to our test code
  // as the test code will block until the main test thread can handle the
  // events.
  template <typename T>
  void ExecuteOffMainThread(T&& func) {
    base::ThreadPool::PostTask(FROM_HERE, base::BindLambdaForTesting(func));
  }

  std::unique_ptr<UserfaultFD> uffd_;

  // To enable FileDescriptorWatcher in unit tests you must use an IO thread.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
};

// We use a mock UserfaultFDHandler handler for testing.
class MockUserfaultFDHandler : public UserfaultFDHandler {
 public:
  MockUserfaultFDHandler() = default;

  MockUserfaultFDHandler(const MockUserfaultFDHandler&) = delete;
  MockUserfaultFDHandler& operator=(const MockUserfaultFDHandler&) = delete;

  ~MockUserfaultFDHandler() override = default;

  MOCK_METHOD3(Pagefault,
               bool(uintptr_t fault_address,
                    PagefaultFlags flags,
                    base::PlatformThreadId tid));
  MOCK_METHOD2(Unmapped, void(uintptr_t range_start, uintptr_t range_end));
  MOCK_METHOD2(Removed, void(uintptr_t range_start, uintptr_t range_end));
  MOCK_METHOD3(Remapped,
               void(uintptr_t old_address,
                    uintptr_t new_address,
                    uint64_t original_length));
  MOCK_METHOD1(Closed, void(int err));
};

uintptr_t GetPageBase(uintptr_t addr) {
  return addr & ~(base::GetPageSize() - 1);
}

void HandleWithZeroRange(UserfaultFD* uffd,
                         uint64_t fault_address,
                         uint64_t size) {
  int64_t zeroed = 0;
  ASSERT_TRUE(uffd->ZeroRange(GetPageBase(fault_address), size, &zeroed));
  ASSERT_EQ(zeroed, static_cast<int64_t>(size));
}

void HandleWithCopyRange(UserfaultFD* uffd,
                         uint64_t fault_address,
                         uint64_t from_address,
                         uint64_t size) {
  int64_t copied = 0;
  ASSERT_TRUE(uffd->CopyToRange(GetPageBase(fault_address), size, from_address,
                                &copied));
  ASSERT_EQ(copied, static_cast<int64_t>(size));
}

// This test will validate that StartWaitingForEvents fails if the uffd is not
// valid at that point.
TEST_F(UserfaultFDTest, TestBadFD) {
  ASSERT_TRUE(CreateUffd());

  // Release takes the FD out of it, meaning it will be left with -1 (a bad fd)
  base::ScopedFD raw_fd(uffd_->ReleaseFD());

  ASSERT_FALSE(uffd_->StartWaitingForEvents(std::move(nullptr)));
}

// This test will validate the userfaultfd behavior with a simple read fault
// which will be resolved by zero filling the page.
TEST_F(UserfaultFDTest, SimpleZeroPageReadFault) {
  ASSERT_TRUE(CreateUffd());

  std::unique_ptr<StrictMock<MockUserfaultFDHandler>> handler(
      new StrictMock<MockUserfaultFDHandler>);

  /* Create a simple mapping with no PTEs */
  ScopedMemory mem(kPageSize);
  ASSERT_TRUE(mem.is_valid());

  ASSERT_TRUE(uffd_->RegisterRange(UserfaultFD::RegisterMode::kRegisterMissing,
                                   mem, kPageSize));

  auto* uffd_ptr = uffd_.get();
  EXPECT_CALL(*handler,
              Pagefault(static_cast<uintptr_t>(mem),
                        UserfaultFDHandler::PagefaultFlags::kReadFault,
                        /* we didn't register for tids */ 0))
      .WillOnce(Invoke([uffd_ptr](uintptr_t fault_address,
                                  UserfaultFDHandler::PagefaultFlags,
                                  base::PlatformThreadId) {
        HandleWithZeroRange(uffd_ptr, fault_address, kPageSize);
        return true;
      }));

  ASSERT_TRUE(uffd_->StartWaitingForEvents(std::move(handler)));

  base::RunLoop run_loop;
  ExecuteOffMainThread([&]() {
    // Now generate a page fault by reading
    // from the page, this will invoke our
    // Pagefault handler above which will
    // zero fill the page for us.
    EXPECT_EQ(*static_cast<int*>(mem), 0);

    run_loop.Quit();
  });

  run_loop.Run();
}

// This test validates that when a fault handler returns false the fault will be
// re-enqued and redelivered later.
TEST_F(UserfaultFDTest, SimpleZeroPageReadFaultRetry) {
  ASSERT_TRUE(CreateUffd());

  std::unique_ptr<StrictMock<MockUserfaultFDHandler>> handler(
      new StrictMock<MockUserfaultFDHandler>);

  /* Create a simple mapping with no PTEs */
  ScopedMemory mem(kPageSize);
  ASSERT_TRUE(mem.is_valid());

  ASSERT_TRUE(uffd_->RegisterRange(UserfaultFD::RegisterMode::kRegisterMissing,
                                   mem, kPageSize));

  // The first fault handle will return false, the second will return true.
  auto* uffd_ptr = uffd_.get();
  EXPECT_CALL(*handler,
              Pagefault(static_cast<uintptr_t>(mem),
                        UserfaultFDHandler::PagefaultFlags::kReadFault,
                        /* we didn't register for tids */ 0))
      .WillOnce(
          Invoke([](uintptr_t fault_address, UserfaultFDHandler::PagefaultFlags,
                    base::PlatformThreadId) { return false; }))
      .WillOnce(Invoke([uffd_ptr](uintptr_t fault_address,
                                  UserfaultFDHandler::PagefaultFlags,
                                  base::PlatformThreadId) {
        HandleWithZeroRange(uffd_ptr, fault_address, kPageSize);
        return true;
      }));

  ASSERT_TRUE(uffd_->StartWaitingForEvents(std::move(handler)));

  base::RunLoop run_loop;
  ExecuteOffMainThread([&]() {
    // Now generate a page fault by reading
    // from the page, this will invoke our
    // Pagefault handler above which will
    // zero fill the page for us.
    EXPECT_EQ(*static_cast<int*>(mem), 0);

    run_loop.Quit();
  });

  run_loop.Run();
}

// This test will cause a simple read fault but will expect the TID to be set.
TEST_F(UserfaultFDTest, SimpleZeroPageReadFaultWithTid) {
  // Create the userfaultfd and tell it we want to see tids.
  ASSERT_TRUE(CreateUffd(UserfaultFD::kFeatureThreadID));

  std::unique_ptr<StrictMock<MockUserfaultFDHandler>> handler(
      new StrictMock<MockUserfaultFDHandler>);

  /* Create a simple mapping with no PTEs */
  ScopedMemory mem(kPageSize);
  ASSERT_TRUE(mem.is_valid());

  ASSERT_TRUE(uffd_->RegisterRange(UserfaultFD::RegisterMode::kRegisterMissing,
                                   mem, kPageSize));

  std::atomic<int32_t> expected_tid{0};

  auto* uffd_ptr = uffd_.get();

  // Because the code that causes the fault runs on a different thread we
  // capture the above atomic var by reference, this allows us to set it before
  // generating a fault so we can verify that we do receive the correct thread
  // id.
  EXPECT_CALL(*handler,
              Pagefault(static_cast<uintptr_t>(mem),
                        UserfaultFDHandler::PagefaultFlags::kReadFault,
                        Eq(ByRef(expected_tid))))
      .WillOnce(Invoke([uffd_ptr](uintptr_t fault_address,
                                  UserfaultFDHandler::PagefaultFlags,
                                  base::PlatformThreadId) {
        HandleWithZeroRange(uffd_ptr, fault_address, kPageSize);
        return true;
      }));

  ASSERT_TRUE(uffd_->StartWaitingForEvents(std::move(handler)));

  base::RunLoop run_loop;
  ExecuteOffMainThread([&]() {
    expected_tid = syscall(__NR_gettid);
    // Now generate a page fault by reading from the page, this will invoke our
    // Pagefault handler above which will zero fill the page for us and we we'll
    // validate the tid we receive against our tid.
    EXPECT_EQ(*static_cast<int*>(mem), 0);

    run_loop.Quit();
  });

  run_loop.Run();
}

// This test will validate userfaultfd with a simple write fault which will be
// resolved by zero filling the page.
TEST_F(UserfaultFDTest, SimpleZeroPageWriteFault) {
  ASSERT_TRUE(CreateUffd());

  std::unique_ptr<StrictMock<MockUserfaultFDHandler>> handler(
      new StrictMock<MockUserfaultFDHandler>);

  /* Create a simple mapping with no PTEs */
  ScopedMemory mem(kPageSize);
  ASSERT_TRUE(mem.is_valid());

  ASSERT_TRUE(uffd_->RegisterRange(UserfaultFD::RegisterMode::kRegisterMissing,
                                   mem, kPageSize));

  auto* uffd_ptr = uffd_.get();
  // We will expect our pagefault handler to be called at the address we
  // allocated as a write fault.
  EXPECT_CALL(*handler,
              Pagefault(static_cast<uintptr_t>(mem),
                        UserfaultFDHandler::PagefaultFlags::kWriteFault,
                        /* we didn't register tid */ 0))
      .WillOnce(Invoke([uffd_ptr](uintptr_t fault_address,
                                  UserfaultFDHandler::PagefaultFlags,
                                  base::PlatformThreadId) {
        HandleWithZeroRange(uffd_ptr, fault_address, kPageSize);
        return true;
      }));

  ASSERT_TRUE(uffd_->StartWaitingForEvents(std::move(handler)));

  base::RunLoop run_loop;
  ExecuteOffMainThread([&]() {
    // Now produce a write fault and verify that the value read back is as
    // expected, the page will be zero filled before our store completes.
    static_cast<int*>(mem)[0] = 8675309;

    // Once we get to this point the fault was zero filled and then retried and
    // the store was completed, validate the value.
    EXPECT_EQ(static_cast<int*>(mem)[0], 8675309);

    // And the remainder of the page will have been zero filled as part of the
    // write fault, the second int in the page will be 0.
    EXPECT_EQ(static_cast<int*>(mem)[1], 0);

    run_loop.Quit();
  });

  run_loop.Run();
}

// This test will validate a simple read pagefault which will be resolved using
// a CopyRange.
TEST_F(UserfaultFDTest, SimpleReadFaultResolveWithCopyPage) {
  ASSERT_TRUE(CreateUffd());

  std::unique_ptr<StrictMock<MockUserfaultFDHandler>> handler(
      new StrictMock<MockUserfaultFDHandler>);

  /* Create a simple mapping with no PTEs */
  ScopedMemory mem(kPageSize);
  ASSERT_TRUE(mem.is_valid());

  ASSERT_TRUE(uffd_->RegisterRange(UserfaultFD::RegisterMode::kRegisterMissing,
                                   mem, kPageSize));

  // We're going to resolve the fault with this page, a page full of 'a'.
  std::vector<uint8_t> buf(kPageSize, 'a');

  auto* uffd_ptr = uffd_.get();

  // we expect that our read fault will happen at our memory address and then we
  // will resolve the fault from the stack page we setup before.
  EXPECT_CALL(*handler,
              Pagefault(static_cast<uintptr_t>(mem),
                        UserfaultFDHandler::PagefaultFlags::kReadFault,
                        /* we didn't register tid */ 0))
      .WillOnce(Invoke([uffd_ptr, &buf](uintptr_t fault_address, uintptr_t,
                                        uintptr_t) {
        HandleWithCopyRange(uffd_ptr, fault_address,
                            reinterpret_cast<uintptr_t>(buf.data()), kPageSize);
        return true;
      }));

  ASSERT_TRUE(uffd_->StartWaitingForEvents(std::move(handler)));

  base::RunLoop run_loop;
  ExecuteOffMainThread([&]() {
    // Now we generate a read fault and we expect the read to be populated by a
    // page full of 'a's
    EXPECT_EQ(*static_cast<char*>(mem), 'a');

    // And confirm the whole page looks as we expect, all 'a's.
    EXPECT_EQ(memcmp(mem, buf.data(), kPageSize), 0);

    run_loop.Quit();
  });

  run_loop.Run();
}

// This test will validate that a large region can be populated using CopyRange
// and that subsequent reads on later pages will not result in a fault.
TEST_F(UserfaultFDTest, ReadFaultResolveWithCopyPageForMultiplePages) {
  ASSERT_TRUE(CreateUffd());

  std::unique_ptr<StrictMock<MockUserfaultFDHandler>> handler(
      new StrictMock<MockUserfaultFDHandler>);

  constexpr size_t kNumPages = 20;
  const size_t kRegionSize = kPageSize * kNumPages;

  /* Create a simple mapping with no PTEs */
  ScopedMemory mem(kRegionSize);
  ASSERT_TRUE(mem.is_valid());

  ASSERT_TRUE(uffd_->RegisterRange(UserfaultFD::RegisterMode::kRegisterMissing,
                                   mem, kRegionSize));

  // We're going to resolve the fault with this page, a page full of 'a'.
  std::vector<uint8_t> buf(kRegionSize, 'a');

  auto* uffd_ptr = uffd_.get();

  // we expect that our read fault will happen at our memory address and then we
  // will resolve the fault from the stack page we setup before.
  EXPECT_CALL(*handler,
              Pagefault(static_cast<uintptr_t>(mem),
                        UserfaultFDHandler::PagefaultFlags::kReadFault,
                        /* we didn't register tid */ 0))
      .WillOnce(Invoke([&](uintptr_t fault_address, uintptr_t, uintptr_t) {
        HandleWithCopyRange(uffd_ptr, fault_address,
                            reinterpret_cast<uintptr_t>(buf.data()),
                            kRegionSize);
        return true;
      }));

  ASSERT_TRUE(uffd_->StartWaitingForEvents(std::move(handler)));

  base::RunLoop run_loop;
  ExecuteOffMainThread([&]() {
    // We generate a read fault. We touch each page, but our handler should only
    // be called once and the WillOnce will make sure of it. The fault handler
    // will have installed the pages for the entire region.
    for (size_t pg_num = 0; pg_num < kNumPages; ++pg_num) {
      EXPECT_EQ(*(static_cast<char*>(mem) + (pg_num * kPageSize)), 'a');

      // And confirm the whole page looks as we expect, all as.
      EXPECT_EQ(memcmp(static_cast<char*>(mem) + (pg_num * kPageSize),
                       buf.data(), kPageSize),
                0);
    }

    run_loop.Quit();
  });

  run_loop.Run();
}

// This test validates that we can repeatedy populate individual pages from a
// larger registered region as each fault happens it will fill that page with a
// repeated character where the character is 'a' + the page number.
TEST_F(UserfaultFDTest,
       ReadFaultResolveWithCopyPageForMultipleIndividualPages) {
  ASSERT_TRUE(CreateUffd());

  std::unique_ptr<StrictMock<MockUserfaultFDHandler>> handler(
      new StrictMock<MockUserfaultFDHandler>);

  constexpr size_t kNumPages = 20;
  const size_t kRegionSize = kPageSize * kNumPages;

  /* Create a simple mapping with no PTEs */
  ScopedMemory mem(kRegionSize);
  ASSERT_TRUE(mem.is_valid());

  ASSERT_TRUE(uffd_->RegisterRange(UserfaultFD::RegisterMode::kRegisterMissing,
                                   mem, kRegionSize));

  auto* uffd_ptr = uffd_.get();

  // We will expect one read fault for each page.
  EXPECT_CALL(*handler,
              Pagefault(_, UserfaultFDHandler::PagefaultFlags::kReadFault,
                        /* we didn't register tid */ 0))
      .Times(Exactly(kNumPages))  // We should be called once for each page.
      .WillRepeatedly(Invoke([&](uintptr_t fault_address,
                                 UserfaultFDHandler::PagefaultFlags,
                                 base::PlatformThreadId) {
        int page_number =
            (GetPageBase(fault_address) - static_cast<uintptr_t>(mem)) /
            kPageSize;
        std::vector<uint8_t> pg_fill_buf(kPageSize, 'a' + page_number);
        // We determine the page number this fault happened in and then we
        // will populate it with 'a' + the page number so we can confirm our
        // fault handler isn't filling more than one page at a time.
        HandleWithCopyRange(uffd_ptr, fault_address,
                            reinterpret_cast<uintptr_t>(pg_fill_buf.data()),
                            kPageSize);
        return true;
      }));

  ASSERT_TRUE(uffd_->StartWaitingForEvents(std::move(handler)));

  base::RunLoop run_loop;
  ExecuteOffMainThread([&]() {
    // We generate a read fault. We touch each page, but our handler should only
    // be called once and the WillOnce will make sure of it. The fault handler
    // will have installed the pages for the entire region.
    for (size_t pg_num = 0; pg_num < kNumPages; ++pg_num) {
      // We generate our fault at a random point within the page and expect to
      // read the character that the fault handler wrote through that entire
      // page.
      off_t random_offset = base::RandInt(0, kPageSize - 1);
      EXPECT_EQ(
          *(static_cast<char*>(mem) + (pg_num * kPageSize + random_offset)),
          'a' + static_cast<char>(pg_num));
    }

    run_loop.Quit();
  });

  run_loop.Run();
}

// This test validates that if we register with userfaultfd on only a portion of
// a mapping we just receive events on that part.
TEST_F(UserfaultFDTest, ReadFaultRegisteredOnPartialRange) {
  ASSERT_TRUE(CreateUffd());

  std::unique_ptr<StrictMock<MockUserfaultFDHandler>> handler(
      new StrictMock<MockUserfaultFDHandler>);

  constexpr size_t kNumPages = 20;

  // We only register with userfaultfd on the first 10 pages.
  constexpr size_t kNumPagesRegistered = 10;

  const size_t kRegisterRegionSize = kPageSize * kNumPagesRegistered;
  const size_t kFullRegionSize = kPageSize * kNumPages;

  /* Create a simple mapping with no PTEs */
  ScopedMemory mem(kFullRegionSize);
  ASSERT_TRUE(mem.is_valid());

  ASSERT_TRUE(uffd_->RegisterRange(UserfaultFD::RegisterMode::kRegisterMissing,
                                   mem, kRegisterRegionSize));

  auto* uffd_ptr = uffd_.get();

  // We will expect one read fault for each page in the registered region.
  EXPECT_CALL(*handler,
              Pagefault(_, UserfaultFDHandler::PagefaultFlags::kReadFault,
                        /* we didn't register tid */ 0))
      .Times(
          Exactly(kNumPagesRegistered))  // We should be called once for each
                                         // page we registered the other pages
                                         // will be zero filled by the kernel.
      .WillRepeatedly(Invoke([&](uintptr_t fault_address,
                                 UserfaultFDHandler::PagefaultFlags,
                                 base::PlatformThreadId) {
        int page_number =
            (GetPageBase(fault_address) - static_cast<uintptr_t>(mem)) /
            kPageSize;
        std::vector<uint8_t> pg_fill_buf(kPageSize, 'a' + page_number);
        // We determine the page number this fault happened in and then we
        // will populate it with 'a' + the page number so we can confirm our
        // fault handler isn't filling more than one page at a time.
        HandleWithCopyRange(uffd_ptr, fault_address,
                            reinterpret_cast<uintptr_t>(pg_fill_buf.data()),
                            kPageSize);
        return true;
      }));

  ASSERT_TRUE(uffd_->StartWaitingForEvents(std::move(handler)));

  base::RunLoop run_loop;
  ExecuteOffMainThread([&]() {
    // We generate a read fault. We touch each page, but our handler should only
    // be called once and the WillOnce will make sure of it. The fault handler
    // will have installed the pages for the entire region.
    for (size_t pg_num = 0; pg_num < kNumPagesRegistered; ++pg_num) {
      // We generate our fault at a random point within the page and expect to
      // read the character that the fault handler wrote through that entire
      // page.
      off_t random_offset = base::RandInt(0, kPageSize - 1);
      EXPECT_EQ(
          *(static_cast<char*>(mem) + (pg_num * kPageSize + random_offset)),
          'a' + static_cast<char>(pg_num));
    }

    // And as we cause faults in the remaining pages they should be zero filled
    // by the kernel because we didn't register with them.
    for (size_t pg_num = kNumPagesRegistered; pg_num < kNumPages; ++pg_num) {
      EXPECT_EQ(*(static_cast<uint8_t*>(mem) + (pg_num * kPageSize)), 0);
    }

    run_loop.Quit();
  });

  run_loop.Run();
}

// This test will validate that writing a value to a page causes a fault that
// will be handled before the store completes. It will only register a portion
// of the VMA we create and we will confirm that the non-registered pages will
// be handled by the kernel.
TEST_F(UserfaultFDTest, WriteFaultRegisteredOnPartialRange) {
  ASSERT_TRUE(CreateUffd());

  std::unique_ptr<StrictMock<MockUserfaultFDHandler>> handler(
      new StrictMock<MockUserfaultFDHandler>);

  constexpr size_t kNumPages = 20;

  // We only register with userfaultfd on the first 10 pages.
  constexpr size_t kNumPagesRegistered = 10;

  const size_t kRegisterRegionSize = kPageSize * kNumPagesRegistered;
  const size_t kFullRegionSize = kPageSize * kNumPages;

  /* Create a simple mapping with no PTEs */
  ScopedMemory mem(kFullRegionSize);
  ASSERT_TRUE(mem.is_valid());

  ASSERT_TRUE(uffd_->RegisterRange(UserfaultFD::RegisterMode::kRegisterMissing,
                                   mem, kRegisterRegionSize));

  auto* uffd_ptr = uffd_.get();

  // We will expect one write fault for each page in the registered region,
  // similar to the read fault test we will fill with 'a' + the page number and
  // or write operation will write an 'X', so we will expect to see the first
  // byte of the page as 'X' with the remainder as that character.
  EXPECT_CALL(*handler,
              Pagefault(_, UserfaultFDHandler::PagefaultFlags::kWriteFault,
                        /* we didn't register tid */ 0))
      .Times(
          Exactly(kNumPagesRegistered))  // We should be called once for each
                                         // page we registered the other pages
                                         // will be zero filled by the kernel.
      .WillRepeatedly(Invoke([&](uintptr_t fault_address,
                                 UserfaultFDHandler::PagefaultFlags,
                                 base::PlatformThreadId) {
        int page_number =
            (GetPageBase(fault_address) - static_cast<uintptr_t>(mem)) /
            kPageSize;
        std::vector<uint8_t> pg_fill_buf(kPageSize, 'a' + page_number);
        // We determine the page number this fault happened in and then we
        // will populate it with 'a' + the page number so we can confirm our
        // fault handler isn't filling more than one page at a time.
        HandleWithCopyRange(uffd_ptr, fault_address,
                            reinterpret_cast<uintptr_t>(pg_fill_buf.data()),
                            kPageSize);
        return true;
      }));

  ASSERT_TRUE(uffd_->StartWaitingForEvents(std::move(handler)));

  base::RunLoop run_loop;
  ExecuteOffMainThread([&]() {
    // Generate a write fault for each page in our range. We will write the byte
    // 'X' as the first byte of the page, the fault handler will populate the
    // whole thing with the single character as described above and when the
    // store completes we will see 'X' followed by the remainder of the page as
    // that character if it was in the region we registered.
    for (size_t pg_num = 0; pg_num < kNumPages; ++pg_num) {
      // This store causes a write fault to the first byte of this page.
      *(static_cast<char*>(mem) + pg_num * kPageSize) = 'X';

      // after the store which caused the fault we expect that byte to be an
      // 'X'.
      EXPECT_EQ(*(static_cast<char*>(mem) + pg_num * kPageSize), 'X');

      // Check the rest of the page contains the character we were expecting.
      // If it was in the range registered it'll be the special character
      // otherwise it'll be filled with zeros by the kernel.
      if (pg_num < kNumPagesRegistered) {
        // Our fault handler ran, so everything after the first byte will be
        // the character we filled.
        for (size_t i = 1; i < kPageSize; ++i) {
          EXPECT_EQ(*(static_cast<char*>(mem) + pg_num * kPageSize + i),
                    'a' + static_cast<char>(pg_num));
        }
      } else {
        // The kernel filled it so everything after that first byte will be 0.
        for (size_t i = 1; i < kPageSize; ++i) {
          EXPECT_EQ(*(static_cast<char*>(mem) + pg_num * kPageSize + i), 0);
        }
      }
    }

    run_loop.Quit();
  });

  run_loop.Run();
}

// This test validates that we receive a Remove event when a PTE is removed.
TEST_F(UserfaultFDTest, SinglePageRemove) {
  ASSERT_TRUE(CreateUffd(UserfaultFD::Features::kFeatureRemove));

  std::unique_ptr<StrictMock<MockUserfaultFDHandler>> handler(
      new StrictMock<MockUserfaultFDHandler>);

  // Create a simple mapping with no PTEs
  ScopedMemory mem(kPageSize);
  ASSERT_TRUE(mem.is_valid());
  const uintptr_t mapping_end = static_cast<uintptr_t>(mem) + kPageSize;

  // Populate the page before hand.
  memset(mem, 'X', kPageSize);

  ASSERT_TRUE(uffd_->RegisterRange(UserfaultFD::RegisterMode::kRegisterMissing,
                                   mem, kPageSize));

  // We will get a Removed call for the range [mem, mapping_end].
  EXPECT_CALL(*handler, Removed(static_cast<uintptr_t>(mem), mapping_end))
      .WillOnce(Return());

  ASSERT_TRUE(uffd_->StartWaitingForEvents(std::move(handler)));

  base::RunLoop run_loop;
  ExecuteOffMainThread([&]() {
    // Now if we zap the region using MADV_DONTNEED we should see a remove
    // event.
    ASSERT_NE(madvise(mem, kPageSize, MADV_DONTNEED), -1);

    run_loop.Quit();
  });

  run_loop.Run();
}

// This test validates that we can receive a remove event on a range we care
// about. This test will register with two pages but remove 3 and our Remove
// event should only notify for the two we're registered on.
TEST_F(UserfaultFDTest, MultiPageRemove) {
  ASSERT_TRUE(CreateUffd(UserfaultFD::Features::kFeatureRemove));

  std::unique_ptr<StrictMock<MockUserfaultFDHandler>> handler(
      new StrictMock<MockUserfaultFDHandler>);

  // Create a simple mapping with no PTEs
  constexpr size_t kNumPages = 3;
  constexpr size_t kNumPagesRegistered = 2;
  const size_t total_size = kNumPages * kPageSize;
  const size_t registered_size = kNumPagesRegistered * kPageSize;

  ScopedMemory mem(total_size);
  ASSERT_TRUE(mem.is_valid());

  // Populate the pages before hand.
  memset(mem, 'X', total_size);

  ASSERT_TRUE(uffd_->RegisterRange(UserfaultFD::RegisterMode::kRegisterMissing,
                                   mem, registered_size));

  // We will get a Removed call for the two pages we registered.
  EXPECT_CALL(*handler, Removed(static_cast<uintptr_t>(mem),
                                static_cast<uintptr_t>(mem) + registered_size))
      .WillOnce(Return());

  ASSERT_TRUE(uffd_->StartWaitingForEvents(std::move(handler)));

  base::RunLoop run_loop;
  ExecuteOffMainThread([&]() {
    // This shouldn't generate a fault, the test would fail if this generates a
    // fault because we don't have an EXPECT_CALL for the fault handler. It
    // doesn't generate a fault because we faulted the pages in earlier.
    ASSERT_EQ(*static_cast<char*>(mem), 'X');

    // Now we zap the entire region causing all 3 PTEs to be blown away, our
    // Removed should notify us about the two pages we registered.
    ASSERT_NE(madvise(mem, total_size, MADV_DONTNEED), -1);

    run_loop.Quit();
  });

  run_loop.Run();
}

// This test validates that we receive an Unmapped event when a mapping is
// unmapped.
TEST_F(UserfaultFDTest, SinglePageUnmap) {
  ASSERT_TRUE(CreateUffd(UserfaultFD::Features::kFeatureUnmap));

  std::unique_ptr<StrictMock<MockUserfaultFDHandler>> handler(
      new StrictMock<MockUserfaultFDHandler>);

  // Create a simple mapping with no PTEs
  ScopedMemory mem(kPageSize);
  ASSERT_TRUE(mem.is_valid());
  const uintptr_t mapping_end = static_cast<uintptr_t>(mem) + kPageSize;

  ASSERT_TRUE(uffd_->RegisterRange(UserfaultFD::RegisterMode::kRegisterMissing,
                                   mem, kPageSize));

  // We will get a Unmapped call for the range [mem, mapping_end].
  EXPECT_CALL(*handler, Unmapped(static_cast<uintptr_t>(mem), mapping_end))
      .WillOnce(Return());

  ASSERT_TRUE(uffd_->StartWaitingForEvents(std::move(handler)));

  base::RunLoop run_loop;
  ExecuteOffMainThread([&]() {
    // Now we unmap the region to observe the Unmapped event.
    mem.Free();

    run_loop.Quit();
  });

  run_loop.Run();
}

// This test validates that we can receive an unmapped event on a range we care
// about. This test will register with two pages but remove 3 and our Remove
// event should only notify for the two we're registered on.
TEST_F(UserfaultFDTest, MultiPageUnmap) {
  ASSERT_TRUE(CreateUffd(UserfaultFD::Features::kFeatureUnmap));

  std::unique_ptr<StrictMock<MockUserfaultFDHandler>> handler(
      new StrictMock<MockUserfaultFDHandler>);

  // Create a simple mapping with no PTEs
  constexpr size_t kNumPages = 3;
  constexpr size_t kNumPagesRegistered = 2;
  const size_t total_size = kNumPages * kPageSize;
  const size_t registered_size = kNumPagesRegistered * kPageSize;

  ScopedMemory mem(total_size);
  ASSERT_TRUE(mem.is_valid());

  // Populate the pages before hand.
  memset(mem, 'X', total_size);

  ASSERT_TRUE(uffd_->RegisterRange(UserfaultFD::RegisterMode::kRegisterMissing,
                                   mem, registered_size));

  // We will get an Unmapped call for the two pages we registered that got
  // unmapped.
  EXPECT_CALL(*handler, Unmapped(static_cast<uintptr_t>(mem),
                                 static_cast<uintptr_t>(mem) + registered_size))
      .WillOnce(Return());

  ASSERT_TRUE(uffd_->StartWaitingForEvents(std::move(handler)));

  base::RunLoop run_loop;
  ExecuteOffMainThread([&]() {
    // This shouldn't generate a fault, the test would fail if this generates a
    // fault because we don't have an EXPECT_CALL for the fault handler. It
    // doesn't generate a fault because we faulted the pages in earlier.
    ASSERT_EQ(*static_cast<char*>(mem), 'X');

    // We take ownership of the memory region so we can unmap the regions we
    // care about.
    void* addr = mem.Release();

    // Now perform the munmap on a portion of the mapping for the region we
    // registered with.
    ASSERT_NE(munmap(addr, registered_size), -1);

    // Finally we can unmap the remaining portion and it should not generate
    // another unmapped event.
    ASSERT_NE(munmap(static_cast<char*>(addr) + registered_size,
                     total_size - registered_size),
              -1);

    run_loop.Quit();
  });

  run_loop.Run();
}

// This test validates that if we unmap just portions of a mapping that we've
// registered we receive an Unmap event for each individual munmap.
TEST_F(UserfaultFDTest, MultiPagePartialUnmap) {
  ASSERT_TRUE(CreateUffd(UserfaultFD::Features::kFeatureUnmap));

  std::unique_ptr<StrictMock<MockUserfaultFDHandler>> handler(
      new StrictMock<MockUserfaultFDHandler>);

  // Create a simple mapping with no PTEs
  constexpr size_t kNumPages = 3;
  constexpr size_t kNumPagesRegistered = 2;
  const size_t total_size = kNumPages * kPageSize;
  const size_t registered_size = kNumPagesRegistered * kPageSize;

  ScopedMemory mem(total_size);
  ASSERT_TRUE(mem.is_valid());

  ASSERT_TRUE(uffd_->RegisterRange(UserfaultFD::RegisterMode::kRegisterMissing,
                                   mem, registered_size));

  // We will two different unmapped calls, one for each page that we're
  // individually unmapping.
  const uintptr_t page1_start = mem;
  const uintptr_t page1_end = page1_start + kPageSize;
  const uintptr_t page2_start = page1_start + kPageSize;
  const uintptr_t page2_end = page2_start + kPageSize;

  // We expect an unmapped event on the first page.
  EXPECT_CALL(*handler, Unmapped(page1_start, page1_end)).WillOnce(Return());

  // And we expect the second unmapped call for the second page.
  EXPECT_CALL(*handler, Unmapped(page2_start, page2_end)).WillOnce(Return());

  ASSERT_TRUE(uffd_->StartWaitingForEvents(std::move(handler)));

  base::RunLoop run_loop;
  ExecuteOffMainThread([&]() {
    // We take ownership of the memory region so we can unmap the regions we
    // care about.
    void* addr = mem.Release();

    // We unmap each page individually this should generate two Unmapped events
    // and the final page wasn't registered so we shouldn't get any events about
    // it.
    for (size_t i = 0; i < kNumPages; ++i) {
      char* page_start = static_cast<char*>(addr) + i * kPageSize;
      ASSERT_NE(munmap(page_start, kPageSize), -1);
    }

    run_loop.Quit();
  });

  run_loop.Run();
}

// This test validates that we receive a remap event when using kFeatureRemap.
TEST_F(UserfaultFDTest, SimpleRemap) {
  ASSERT_TRUE(CreateUffd(static_cast<UserfaultFD::Features>(
      UserfaultFD::Features::kFeatureRemap)));

  std::unique_ptr<StrictMock<MockUserfaultFDHandler>> handler(
      new StrictMock<MockUserfaultFDHandler>);

  ScopedMemory mem(kPageSize);
  ASSERT_TRUE(mem.is_valid());

  ASSERT_TRUE(uffd_->RegisterRange(UserfaultFD::RegisterMode::kRegisterMissing,
                                   mem, kPageSize));

  // We will get a Remapped call for the range mem of length kPageSize.
  EXPECT_CALL(*handler, Remapped(static_cast<uintptr_t>(mem), _,
                                 /* original length */ kPageSize))
      .WillOnce(Return());

  ASSERT_TRUE(uffd_->StartWaitingForEvents(std::move(handler)));

  base::RunLoop run_loop;
  ExecuteOffMainThread([&]() {
    void* new_addr = mem.Remap(2 * kPageSize);
    ASSERT_NE(new_addr, MAP_FAILED);

    run_loop.Quit();
  });

  run_loop.Run();
}

// This test validates that using kFeatureRemap you observe a remap event and
// any access to the region after the remap will result in a fault at the new
// address.
TEST_F(UserfaultFDTest, RemapAndFaultAtNewAddress) {
  ASSERT_TRUE(CreateUffd(static_cast<UserfaultFD::Features>(
      UserfaultFD::Features::kFeatureRemap)));

  std::unique_ptr<StrictMock<MockUserfaultFDHandler>> handler(
      new StrictMock<MockUserfaultFDHandler>);

  // Because we don't know upfront where the remap will place things we capture
  // them into a variable that goes into the matcher by reference.
  std::atomic<uintptr_t> remapped_start{0};
  std::atomic<uintptr_t> second_page_start{0};
  std::atomic<uintptr_t> observed_remap{0};

  ScopedMemory mem(kPageSize);
  ASSERT_TRUE(mem.is_valid());

  ASSERT_TRUE(uffd_->RegisterRange(UserfaultFD::RegisterMode::kRegisterMissing,
                                   mem, kPageSize));

  // We will get a Remapped call for the range mem of length kPageSize.
  EXPECT_CALL(*handler, Remapped(static_cast<uintptr_t>(mem), _,
                                 /* original length */ kPageSize))
      .WillOnce([&](uintptr_t, uintptr_t new_addr, uintptr_t) {
        // We capture our observed address, we have to do it this way
        // because this callback would be racing with the store of the remap
        // address below. So we just observe it and then validate it at the
        // end of the test.
        observed_remap = new_addr;
        return true;
      });

  // We will expect a fault event from our load instruction that is performed
  // after the remap. Because we don't know exactly where the remap will happen
  // to (because we're not using MREMAP_FIXED), we capture a variable by
  // reference that will be set to the address after remap, this allows us to
  // confirm that the Pagefault call we want is the correct one.
  auto* uffd_ptr = uffd_.get();
  EXPECT_CALL(*handler,
              Pagefault(Eq(ByRef(remapped_start)),
                        UserfaultFDHandler::PagefaultFlags::kReadFault,
                        /* we didn't register for tids */ 0))
      .WillOnce(Invoke([uffd_ptr](uintptr_t fault_address,
                                  UserfaultFDHandler::PagefaultFlags,
                                  base::PlatformThreadId) {
        HandleWithZeroRange(uffd_ptr, fault_address, kPageSize);
        return true;
      }));

  // And because the userfaultfd is attached to the VMA when it's remapped and
  // grown we also have a userfaultfd registered on the new second page.
  EXPECT_CALL(*handler,
              Pagefault(Eq(ByRef(second_page_start)),
                        UserfaultFDHandler::PagefaultFlags::kReadFault,
                        /* we didn't register for tids */ 0))
      .WillOnce(Invoke([uffd_ptr](uintptr_t fault_address,
                                  UserfaultFDHandler::PagefaultFlags,
                                  base::PlatformThreadId) {
        HandleWithZeroRange(uffd_ptr, fault_address, kPageSize);
        return true;
      }));

  ASSERT_TRUE(uffd_->StartWaitingForEvents(std::move(handler)));

  base::RunLoop run_loop;
  ExecuteOffMainThread([&]() {
    mem.Remap(2 * kPageSize);
    ASSERT_TRUE(mem.is_valid());

    // We have to store where we remapped to so our EXCEPT_CALL on the Pagefault
    // can validate that the pagefault is happening at the expected place.
    remapped_start = static_cast<uintptr_t>(mem);
    second_page_start = remapped_start + kPageSize;

    // Now we generate a read fault at the new address and our fault handler
    // will zero fill it.
    EXPECT_EQ(*static_cast<char*>(mem), 0);

    // Because we grew the mapping as part of mremap we should also be able to
    // trigger a fault at the next page.
    EXPECT_EQ(*(static_cast<char*>(mem) + kPageSize), 0);

    run_loop.Quit();
  });

  run_loop.Run();

  // Now validate the observed remap address.
  EXPECT_EQ(observed_remap, remapped_start);
}

}  // namespace userspace_swap
}  // namespace memory
}  // namespace ash
