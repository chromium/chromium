// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/delete_with_retry.h"

#include <windows.h>

#include <memory>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mini_installer {

namespace {

// A class for mocking DeleteWithRetry's sleep hook.
class MockSleepHook {
 public:
  MockSleepHook() = default;
  MockSleepHook(const MockSleepHook&) = delete;
  MockSleepHook& operator=(const MockSleepHook&) = delete;
  virtual ~MockSleepHook() = default;

  MOCK_METHOD(void, Sleep, ());
};

// A helper for temporarily connecting a specific MockSleepHook to
// DeleteWithRetry's sleep hook. Only one such instance may be alive at any
// given time.
class ScopedSleepHook {
 public:
  explicit ScopedSleepHook(MockSleepHook* hook) : hook_(hook) {
    EXPECT_EQ(SetRetrySleepHookForTesting(&ScopedSleepHook::SleepHook, this),
              nullptr);
  }
  ScopedSleepHook(const ScopedSleepHook&) = delete;
  ScopedSleepHook& operator=(const ScopedSleepHook&) = delete;
  ~ScopedSleepHook() {
    EXPECT_EQ(SetRetrySleepHookForTesting(nullptr, nullptr),
              &ScopedSleepHook::SleepHook);
  }

 private:
  static void SleepHook(void* context) {
    reinterpret_cast<ScopedSleepHook*>(context)->DoSleep();
  }
  void DoSleep() { hook_->Sleep(); }

  MockSleepHook* hook_;
};

}  // namespace

class DeleteWithRetryTest : public ::testing::Test {
 protected:
  DeleteWithRetryTest() = default;

  // ::testing::Test:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }
  void TearDown() override { EXPECT_TRUE(temp_dir_.Delete()); }

  const base::FilePath& TestDir() const { return temp_dir_.GetPath(); }

  base::ScopedTempDir temp_dir_;
};

// Tests that deleting an item in a directory that doesn't exist succeeds.
TEST_F(DeleteWithRetryTest, DeleteNoDir) {
  int attempts = 0;
  EXPECT_TRUE(DeleteWithRetry(TestDir()
                                  .Append(FILE_PATH_LITERAL("nodir"))
                                  .Append(FILE_PATH_LITERAL("noitem"))
                                  .value()
                                  .c_str(),
                              attempts));
  EXPECT_EQ(attempts, 1);
}

// Tests that deleting an item that doesn't exist in a directory that does exist
// succeeds.
TEST_F(DeleteWithRetryTest, DeleteNoFile) {
  const base::FilePath path = TestDir().Append(FILE_PATH_LITERAL("noitem"));
  int attempts = 0;
  ASSERT_TRUE(DeleteWithRetry(path.value().c_str(), attempts));
  EXPECT_EQ(attempts, 1);
}

// Tests that deleting a file succeeds.
TEST_F(DeleteWithRetryTest, DeleteFile) {
  const base::FilePath path = TestDir().Append(FILE_PATH_LITERAL("file"));
  ASSERT_TRUE(base::WriteFile(path, std::string_view()));
  int attempts = 0;
  ASSERT_TRUE(DeleteWithRetry(path.value().c_str(), attempts));
  EXPECT_GE(attempts, 1);
  EXPECT_FALSE(base::PathExists(path));
}

// Tests that deleting a read-only file succeeds.
TEST_F(DeleteWithRetryTest, DeleteReadonlyFile) {
  const base::FilePath path = TestDir().Append(FILE_PATH_LITERAL("file"));
  ASSERT_TRUE(base::WriteFile(path, std::string_view()));
  DWORD attributes = ::GetFileAttributes(path.value().c_str());
  ASSERT_NE(attributes, INVALID_FILE_ATTRIBUTES) << ::GetLastError();
  ASSERT_NE(::SetFileAttributes(path.value().c_str(),
                                attributes | FILE_ATTRIBUTE_READONLY),
            0)
      << ::GetLastError();
  int attempts = 0;
  ASSERT_TRUE(DeleteWithRetry(path.value().c_str(), attempts));
  EXPECT_GE(attempts, 1);
  EXPECT_FALSE(base::PathExists(path));
}

// Tests that deleting an empty directory succeeds.
TEST_F(DeleteWithRetryTest, DeleteEmptyDir) {
  const base::FilePath path = TestDir().Append(FILE_PATH_LITERAL("dir"));
  ASSERT_TRUE(base::CreateDirectory(path));
  int attempts = 0;
  ASSERT_TRUE(DeleteWithRetry(path.value().c_str(), attempts));
  EXPECT_GE(attempts, 1);
  EXPECT_FALSE(base::PathExists(path));
}

// Tests that deleting a non-empty directory fails.
TEST_F(DeleteWithRetryTest, DeleteNonEmptyDir) {
  const base::FilePath path = TestDir().Append(FILE_PATH_LITERAL("dir"));
  ASSERT_TRUE(base::CreateDirectory(path));
  ASSERT_TRUE(base::WriteFile(path.Append(FILE_PATH_LITERAL("file")),
                              std::string_view()));
  {
    ::testing::StrictMock<MockSleepHook> mock_hook;
    ScopedSleepHook hook(&mock_hook);
    int attempts = 0;
    EXPECT_CALL(mock_hook, Sleep()).Times(99);
    ASSERT_FALSE(DeleteWithRetry(path.value().c_str(), attempts));
    EXPECT_EQ(attempts, 100);
  }
  EXPECT_TRUE(base::PathExists(path));
}

// Tests that deleting a non-empty directory succeeds once a file within is
// deleted.
TEST_F(DeleteWithRetryTest, DeleteDirThatEmpties) {
  const base::FilePath path = TestDir().Append(FILE_PATH_LITERAL("dir"));
  ASSERT_TRUE(base::CreateDirectory(path));
  const base::FilePath file = path.Append(FILE_PATH_LITERAL("file"));
  ASSERT_TRUE(base::WriteFile(file, std::string_view()));
  {
    ::testing::NiceMock<MockSleepHook> mock_hook;
    ScopedSleepHook hook(&mock_hook);
    int attempts = 0;
    EXPECT_CALL(mock_hook, Sleep()).WillOnce([&file]() {
      ::DeleteFile(file.value().c_str());
    });
    ASSERT_TRUE(DeleteWithRetry(path.value().c_str(), attempts));
    EXPECT_LT(attempts, 100);
  }
  EXPECT_FALSE(base::PathExists(path));
}

// Tests that deleting a file mapped into a process's address space triggers
// a retry that succeeds after the file is closed.
TEST_F(DeleteWithRetryTest, DeleteMappedFile) {
  const base::FilePath path = TestDir().Append(FILE_PATH_LITERAL("file"));
  ASSERT_TRUE(base::WriteFile(path, std::string_view("i miss you")));

  // Open the file for read-only access; allowing others to do anything.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE);
  ASSERT_TRUE(file.IsValid()) << file.error_details();

  // Map the file into the process's address space, thereby preventing deletes.
  auto mapped_file = std::make_unique<base::MemoryMappedFile>();
  ASSERT_TRUE(mapped_file->Initialize(std::move(file)));

  // Try to delete the file, expecting that a retry-induced sleep takes place.
  // Unmap and close the file when that happens so that the retry succeeds.
  {
    ::testing::NiceMock<MockSleepHook> mock_hook;
    EXPECT_CALL(mock_hook, Sleep()).WillOnce([&mapped_file]() {
      mapped_file.reset();
    });
    ScopedSleepHook hook(&mock_hook);
    int attempts = 0;
    ASSERT_TRUE(DeleteWithRetry(path.value().c_str(), attempts));
    EXPECT_GE(attempts, 2);
  }
  EXPECT_FALSE(base::PathExists(path));
}

// Tests that deleting a file with an open handle succeeds after the file is
// closed.
TEST_F(DeleteWithRetryTest, DeleteInUseFile) {
  const base::FilePath path = TestDir().Append(FILE_PATH_LITERAL("file"));
  ASSERT_TRUE(base::WriteFile(path, std::string_view("i miss you")));

  // Open the file for read-only access; allowing others to do anything.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE);
  ASSERT_TRUE(file.IsValid()) << file.error_details();

  // Try to delete the file, expecting that a retry-induced sleep takes place.
  // Close the file when that happens so that the retry succeeds.
  {
    ::testing::NiceMock<MockSleepHook> mock_hook;
    EXPECT_CALL(mock_hook, Sleep()).WillOnce([&file]() { file.Close(); });
    ScopedSleepHook hook(&mock_hook);
    int attempts = 0;
    ASSERT_TRUE(DeleteWithRetry(path.value().c_str(), attempts));
    EXPECT_GE(attempts, 2);
  }
  EXPECT_FALSE(base::PathExists(path));
}

// Test that a read-only file that cannot be opened for deletion takes at least
// one retry.
TEST_F(DeleteWithRetryTest, DeleteReadOnlyNoSharing) {
  const base::FilePath path = TestDir().Append(FILE_PATH_LITERAL("file"));
  ASSERT_TRUE(base::WriteFile(path, std::string_view("i miss you")));

  // Make it read-only.
  DWORD attributes = ::GetFileAttributes(path.value().c_str());
  ASSERT_NE(attributes, INVALID_FILE_ATTRIBUTES) << ::GetLastError();
  ASSERT_NE(::SetFileAttributes(path.value().c_str(),
                                attributes | FILE_ATTRIBUTE_READONLY),
            0)
      << ::GetLastError();

  // Open the file for read-only access; allowing others to do anything.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE);
  ASSERT_TRUE(file.IsValid()) << file.error_details();

  // Try to delete the file, expecting that a retry-induced sleep takes place.
  // Close the file so that a retry succeeds.
  {
    ::testing::NiceMock<MockSleepHook> mock_hook;
    EXPECT_CALL(mock_hook, Sleep()).WillOnce([&file]() { file.Close(); });
    ScopedSleepHook hook(&mock_hook);
    int attempts = 0;
    ASSERT_TRUE(DeleteWithRetry(path.value().c_str(), attempts));
    EXPECT_GT(attempts, 1);
  }
  EXPECT_FALSE(base::PathExists(path));
}

// Tests that deleting fails after all retries are used up.
TEST_F(DeleteWithRetryTest, MaxRetries) {
  const base::FilePath path = TestDir().Append(FILE_PATH_LITERAL("file"));
  ASSERT_TRUE(base::WriteFile(path, std::string_view("i miss you")));

  // Open the file for read-only access without allowing deletes.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid()) << file.error_details();

  // Expect all 100 attempts to fail, with 99 sleeps betwixt them.
  {
    ::testing::StrictMock<MockSleepHook> mock_hook;
    ScopedSleepHook hook(&mock_hook);
    int attempts = 0;
    EXPECT_CALL(mock_hook, Sleep()).Times(99);
    ASSERT_FALSE(DeleteWithRetry(path.value().c_str(), attempts));
    EXPECT_EQ(attempts, 100);
  }
  EXPECT_TRUE(base::PathExists(path));
}

// Test that success on the last retry is reported correctly.
TEST_F(DeleteWithRetryTest, LastRetrySucceeds) {
  const base::FilePath path = TestDir().Append(FILE_PATH_LITERAL("file"));
  ASSERT_TRUE(base::WriteFile(path, std::string_view("i miss you")));

  // Open the file for read-only access; allowing others to do anything.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE);
  ASSERT_TRUE(file.IsValid()) << file.error_details();

  // Try to delete the file, expecting that a retry-induced sleep takes place.
  // Close the file on the 99th retry so that the last attempt succeeds.
  {
    ::testing::InSequence sequence;
    ::testing::StrictMock<MockSleepHook> mock_hook;
    EXPECT_CALL(mock_hook, Sleep()).Times(98);
    EXPECT_CALL(mock_hook, Sleep()).WillOnce([&file]() { file.Close(); });
    ScopedSleepHook hook(&mock_hook);
    int attempts = 0;
    ASSERT_TRUE(DeleteWithRetry(path.value().c_str(), attempts));
    EXPECT_EQ(attempts, 100);
  }
  EXPECT_FALSE(base::PathExists(path));
}

}  // namespace mini_installer
