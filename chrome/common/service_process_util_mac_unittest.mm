// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/service_process_util.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_pump_type.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "chrome/common/mac/launchd.h"
#include "chrome/common/mac/mock_launchd.h"
#include "testing/gtest/include/gtest/gtest.h"

class ServiceProcessStateFileManipulationTest : public ::testing::Test {
 public:
  void TrashFunc(const base::FilePath& src) {
    NSURL* url = [NSURL fileURLWithPath:base::SysUTF8ToNSString(src.value())];
    ASSERT_TRUE(url);
    NSURL* resultingItemURL = nil;
    BOOL success =
        [[NSFileManager defaultManager] trashItemAtURL:url
                                      resultingItemURL:&resultingItemURL
                                                 error:nil];
    ASSERT_TRUE(success);
    trashed_url_.reset([resultingItemURL retain]);
  }

 protected:
  ServiceProcessStateFileManipulationTest()
      : io_thread_("ServiceProcessStateFileManipulationTest_IO") {}

  void SetUp() override {
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    ASSERT_TRUE(io_thread_.StartWithOptions(options));
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(MockLaunchd::MakeABundle(GetTempDirPath(), "Test",
                                         &bundle_path_, &executable_path_));
    mock_launchd_ = std::make_unique<MockLaunchd>(
        executable_path_, task_environment_.GetMainThreadTaskRunner(),
        run_loop_.QuitClosure(), true);
    scoped_launchd_instance_ =
        std::make_unique<Launchd::ScopedInstance>(mock_launchd_.get());
    ASSERT_TRUE(service_process_state_.Initialize());
    ASSERT_TRUE(service_process_state_.SignalReady(
        io_thread_.task_runner().get(), base::OnceClosure()));
    task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
        FROM_HERE, run_loop_.QuitWhenIdleClosure(),
        TestTimeouts::action_max_timeout());
  }

  const MockLaunchd* mock_launchd() const { return mock_launchd_.get(); }
  const base::FilePath& executable_path() const { return executable_path_; }
  const base::FilePath& bundle_path() const { return bundle_path_; }
  const base::FilePath& GetTempDirPath() const { return temp_dir_.GetPath(); }

  base::SingleThreadTaskRunner* GetIOTaskRunner() {
    return io_thread_.task_runner().get();
  }
  void Run() { run_loop_.Run(); }

  base::scoped_nsobject<NSURL> trashed_url_;

 private:
  base::ScopedTempDir temp_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  base::RunLoop run_loop_;
  base::Thread io_thread_;
  base::FilePath executable_path_, bundle_path_;
  std::unique_ptr<MockLaunchd> mock_launchd_;
  std::unique_ptr<Launchd::ScopedInstance> scoped_launchd_instance_;
  ServiceProcessState service_process_state_;
};

void DeleteFunc(const base::FilePath& file) {
  EXPECT_TRUE(base::DeletePathRecursively(file));
}

void MoveFunc(const base::FilePath& from, const base::FilePath& to) {
  EXPECT_TRUE(base::Move(from, to));
}

void ChangeAttr(const base::FilePath& from, int mode) {
  EXPECT_EQ(chmod(from.value().c_str(), mode), 0);
}

class ScopedAttributesRestorer {
 public:
  ScopedAttributesRestorer(const base::FilePath& path, int mode)
      : path_(path), mode_(mode) {}
  ~ScopedAttributesRestorer() { ChangeAttr(path_, mode_); }

 private:
  base::FilePath path_;
  int mode_;
};

TEST_F(ServiceProcessStateFileManipulationTest, VerifyLaunchD) {
  // There have been problems where launchd has gotten into a bad state, usually
  // because something had deleted all the files in /tmp. launchd depends on
  // a Unix Domain Socket that it creates at /tmp/launchd*/sock.
  // The symptom of this problem is that the service process connect fails
  // on Mac and "launch_msg(): Socket is not connected" appears.
  // This test is designed to make sure that launchd is working.
  // http://crbug/75518
  // Note: This particular problem no longer affects launchd in 10.10+, since
  // there is no user owned launchd process and sockets are no longer made at
  // /tmp/launchd*/sock. This test is still useful as a sanity check to make
  // sure that launchd appears to be working.

  base::CommandLine cl(base::FilePath("/bin/launchctl"));
  cl.AppendArg("limit");

  std::string output;
  int exit_code = -1;
  ASSERT_TRUE(base::GetAppOutputWithExitCode(cl, &output, &exit_code) &&
              exit_code == 0)
      << " exit_code:" << exit_code << " " << output;
}

// Flaky: https://crbug.com/903823
TEST_F(ServiceProcessStateFileManipulationTest, DISABLED_DeleteFile) {
  GetIOTaskRunner()->PostTask(FROM_HERE,
                              base::BindOnce(&DeleteFunc, executable_path()));
  Run();
  ASSERT_TRUE(mock_launchd()->remove_called());
  ASSERT_TRUE(mock_launchd()->delete_called());
}

TEST_F(ServiceProcessStateFileManipulationTest, DeleteBundle) {
  GetIOTaskRunner()->PostTask(FROM_HERE,
                              base::BindOnce(&DeleteFunc, bundle_path()));
  Run();
  ASSERT_TRUE(mock_launchd()->remove_called());
  ASSERT_TRUE(mock_launchd()->delete_called());
}

TEST_F(ServiceProcessStateFileManipulationTest, MoveBundle) {
  base::FilePath new_loc = GetTempDirPath().AppendASCII("MoveBundle");
  GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&MoveFunc, bundle_path(), new_loc));
  Run();
  ASSERT_TRUE(mock_launchd()->restart_called());
  ASSERT_TRUE(mock_launchd()->write_called());
}

TEST_F(ServiceProcessStateFileManipulationTest, MoveFile) {
  base::FilePath new_loc = GetTempDirPath().AppendASCII("MoveFile");
  GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&MoveFunc, executable_path(), new_loc));
  Run();
  ASSERT_TRUE(mock_launchd()->remove_called());
  ASSERT_TRUE(mock_launchd()->delete_called());
}

TEST_F(ServiceProcessStateFileManipulationTest, TrashBundle) {
  GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceProcessStateFileManipulationTest::TrashFunc,
                     base::Unretained(this), bundle_path()));
  Run();
  ASSERT_TRUE(mock_launchd()->remove_called());
  ASSERT_TRUE(mock_launchd()->delete_called());
  std::string path(base::SysNSStringToUTF8([trashed_url_ path]));
  base::FilePath file_path(path);
  ASSERT_TRUE(base::DeletePathRecursively(file_path));
}

TEST_F(ServiceProcessStateFileManipulationTest, ChangeAttr) {
  ScopedAttributesRestorer restorer(bundle_path(), 0777);
  GetIOTaskRunner()->PostTask(FROM_HERE,
                              base::BindOnce(&ChangeAttr, bundle_path(), 0222));
  Run();
  ASSERT_TRUE(mock_launchd()->remove_called());
  ASSERT_TRUE(mock_launchd()->delete_called());
}
