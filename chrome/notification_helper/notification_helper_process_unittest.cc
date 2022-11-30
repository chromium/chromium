// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests launching notification_helper.exe by the OS via the registry.
//
// An advanced version of this test is
// chrome/browser/notifications/win/notification_helper_launches_chrome_unittest.cc,
// which additionally tests if chrome.exe can be successfully launched by
// notification_helper.exe via the NotificationActivator::Activate function.
//
// Different from this test being compiled into
// notification_helper_unittests.exe, the advanced test is compiled into
// unit_tests.exe. This is because the advanced test requires data dependency on
// chrome.exe which unit_tests.exe already has, and it's undesired to make
// notification_helper_unittests.exe have data dependency on chrome.exe.

#include <memory>
#include <string>

#include <wrl/client.h>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/process/process_iterator.h"
#include "base/test/test_timeouts.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_types.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/setup/install_worker.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Returns the process with name |name| if it is found.
base::Process FindProcess(const std::wstring& name) {
  unsigned int pid;
  {
    base::NamedProcessIterator iter(name, nullptr);
    const auto* entry = iter.NextProcessEntry();
    if (!entry)
      return base::Process();
    pid = entry->pid();
  }

  auto process = base::Process::Open(pid);
  if (!process.IsValid())
    return process;

  // Since the process could go away suddenly before we open a handle to it,
  // it's possible that a different process was just opened and assigned the
  // same PID due to aggressive PID reuse. Now that a handle is held to *some*
  // process, take another run through the snapshot to see if the process with
  // this PID has the right exe name.
  base::NamedProcessIterator iter(name, nullptr);
  while (const auto* entry = iter.NextProcessEntry()) {
    if (entry->pid() == pid)
      return process;  // PID was not reused since the PID's match.
  }
  return base::Process();  // The PID was reused.
}

}  // namespace

class NotificationHelperTest : public testing::Test {
 public:
  NotificationHelperTest(const NotificationHelperTest&) = delete;
  NotificationHelperTest& operator=(const NotificationHelperTest&) = delete;

 protected:
  NotificationHelperTest() : root_(HKEY_CURRENT_USER) {}

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(RegisterServer());
  }

  void TearDown() override {
    ASSERT_NO_FATAL_FAILURE(UnregisterServer());
  }

 private:
  // Registers notification_helper.exe as the server.
  void RegisterServer() {
    ASSERT_TRUE(scoped_com_initializer_.Succeeded());

    // Notification_helper.exe is in the build output directory next to this
    // test executable, as the test build target has a data_deps dependency on
    // it.
    base::FilePath dir_exe;
    ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &dir_exe));
    base::FilePath notification_helper_path =
        dir_exe.Append(installer::kNotificationHelperExe);

    work_item_list_ =
        base::WrapUnique<WorkItemList>(WorkItem::CreateWorkItemList());

    installer::AddNativeNotificationWorkItems(root_, notification_helper_path,
                                              work_item_list_.get());

    ASSERT_TRUE(work_item_list_->Do());
  }

  // Unregisters the server by rolling back the work item list.
  void UnregisterServer() {
    if (work_item_list_)
      work_item_list_->Rollback();
  }

  // Predefined handle to the registry.
  const HKEY root_;

  // A list of work items on the registry.
  std::unique_ptr<WorkItemList> work_item_list_;

  base::win::ScopedCOMInitializer scoped_com_initializer_;
};

TEST_F(NotificationHelperTest, NotificationHelperServerTest) {
  // There isn't a way to directly correlate the notification_helper.exe server
  // to this test. So we need to hunt for the server.

  // Make sure there is no notification_helper process running around.
  base::Process helper = FindProcess(installer::kNotificationHelperExe);
  ASSERT_FALSE(helper.IsValid());

  Microsoft::WRL::ComPtr<IUnknown> notification_activator;
  ASSERT_HRESULT_SUCCEEDED(::CoCreateInstance(
      install_static::GetToastActivatorClsid(), nullptr, CLSCTX_LOCAL_SERVER,
      IID_PPV_ARGS(&notification_activator)));
  ASSERT_TRUE(notification_activator);

  // The server is now invoked upon the request of creating the object instance.
  // The server module now holds a reference of the instance object, the
  // notification_helper.exe process is alive waiting for that reference to be
  // released.
  helper = FindProcess(installer::kNotificationHelperExe);
  ASSERT_TRUE(helper.IsValid());

  // Release the instance object. Now that the last (and the only) instance
  // object of the module is released, the event living in the server
  // process is signaled, which allows the server process to exit.
  notification_activator.Reset();
  ASSERT_TRUE(
      helper.WaitForExitWithTimeout(TestTimeouts::action_timeout(), nullptr));
}
