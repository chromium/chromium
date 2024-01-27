// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/sync/key_storage_linux.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

// We use a fake to avoid calling a real backend. We'll make calls to it and
// test that the wrapping methods post accordingly.
class FakeKeyStorageLinux : public KeyStorageLinux {
 public:
  explicit FakeKeyStorageLinux(base::SequencedTaskRunner* task_runner)
      : task_runner_(task_runner) {}

  FakeKeyStorageLinux(const FakeKeyStorageLinux&) = delete;
  FakeKeyStorageLinux& operator=(const FakeKeyStorageLinux&) = delete;

  ~FakeKeyStorageLinux() override = default;

 protected:
  bool Init() override { return true; }
  std::optional<std::string> GetKeyImpl() override {
    return std::string("1234");
  }

  base::SequencedTaskRunner* GetTaskRunner() override { return task_runner_; }

 private:
  raw_ptr<base::SequencedTaskRunner> task_runner_;
};

class KeyStorageLinuxTest : public testing::Test {
 public:
  KeyStorageLinuxTest() = default;

  KeyStorageLinuxTest(const KeyStorageLinuxTest&) = delete;
  KeyStorageLinuxTest& operator=(const KeyStorageLinuxTest&) = delete;

  ~KeyStorageLinuxTest() override = default;
};

TEST_F(KeyStorageLinuxTest, SkipPostingToSameTaskRunner) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner(
      new base::TestSimpleTaskRunner());
  FakeKeyStorageLinux key_storage(task_runner.get());

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(&KeyStorageLinux::GetKey),
                                base::Unretained(&key_storage)));

  // This should not deadlock.
  task_runner->RunUntilIdle();
}

TEST_F(KeyStorageLinuxTest, IgnoreTaskRunnerIfNull) {
  FakeKeyStorageLinux key_storage(nullptr);
  // This should not deadlock or crash.
  ASSERT_EQ(std::string("1234"), key_storage.GetKey());
}
