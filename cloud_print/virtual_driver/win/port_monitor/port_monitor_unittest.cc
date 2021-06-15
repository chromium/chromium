// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/virtual_driver/win/port_monitor/port_monitor.h"

#include <stddef.h>
#include <winspool.h>

#include <string>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "cloud_print/virtual_driver/win/port_monitor/spooler_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cloud_print {

const wchar_t kChromeExePath[] = L"google\\chrome\\application\\chrometest.exe";
const wchar_t kChromeExePathRegValue[] = L"PathToChromeTestExe";
const wchar_t kChromeProfilePathRegValue[] = L"PathToChromeTestProfile";
const wchar_t kPrintCommandRegValue[] = L"TestPrintCommand";
const bool kIsUnittest = true;

namespace {

const wchar_t kAlternateChromeExePath[] =
    L"google\\chrome\\application\\chrometestalternate.exe";
const wchar_t kTestPrintCommand[] = L"testprintcommand.exe";

const wchar_t kCloudPrintRegKey[] = L"Software\\Google\\CloudPrint";

}  // namespace

class PortMonitorTest : public testing::Test {
 public:
  PortMonitorTest() = default;
  PortMonitorTest(const PortMonitorTest&) = delete;
  PortMonitorTest& operator=(const PortMonitorTest&) = delete;

 protected:
  // Creates a registry entry pointing at a chrome
  virtual void SetUpChromeExeRegistry() {
    // Create a temporary chrome.exe location value.
    base::win::RegKey key(HKEY_CURRENT_USER, cloud_print::kCloudPrintRegKey,
                          KEY_ALL_ACCESS);

    base::FilePath path;
    base::PathService::Get(base::DIR_LOCAL_APP_DATA, &path);
    path = path.Append(kAlternateChromeExePath);
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(cloud_print::kChromeExePathRegValue,
                                            path.value().c_str()));
    base::FilePath temp;
    base::PathService::Get(base::DIR_TEMP, &temp);
    // Write any dir here.
    ASSERT_EQ(ERROR_SUCCESS,
              key.WriteValue(cloud_print::kChromeProfilePathRegValue,
                             temp.value().c_str()));

    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(cloud_print::kPrintCommandRegValue,
                                            kTestPrintCommand));
  }
  // Deletes the registry entry created in SetUpChromeExeRegistry
  virtual void DeleteChromeExeRegistry() {
    base::win::RegKey key(HKEY_CURRENT_USER, cloud_print::kCloudPrintRegKey,
                          KEY_ALL_ACCESS);
    key.DeleteValue(cloud_print::kChromeExePathRegValue);
    key.DeleteValue(cloud_print::kChromeProfilePathRegValue);
    key.DeleteValue(cloud_print::kPrintCommandRegValue);
  }

  virtual void CreateTempChromeExeFiles() {
    base::FilePath path;
    base::PathService::Get(base::DIR_LOCAL_APP_DATA, &path);
    base::FilePath main_path = path.Append(kChromeExePath);
    ASSERT_TRUE(base::CreateDirectory(main_path));
    base::FilePath alternate_path = path.Append(kAlternateChromeExePath);
    ASSERT_TRUE(base::CreateDirectory(alternate_path));
  }

  virtual void DeleteTempChromeExeFiles() {
    base::FilePath path;
    base::PathService::Get(base::DIR_LOCAL_APP_DATA, &path);
    base::FilePath main_path = path.Append(kChromeExePath);
    ASSERT_TRUE(base::DeletePathRecursively(main_path));
    base::PathService::Get(base::DIR_LOCAL_APP_DATA, &path);
    base::FilePath alternate_path = path.Append(kAlternateChromeExePath);
    ASSERT_TRUE(base::DeletePathRecursively(alternate_path));
  }

  void SetUp() override { SetUpChromeExeRegistry(); }

  void TearDown() override { DeleteChromeExeRegistry(); }
};

TEST_F(PortMonitorTest, GetChromeExePathTest) {
  CreateTempChromeExeFiles();
  base::FilePath chrome_path = cloud_print::GetChromeExePath();
  EXPECT_FALSE(chrome_path.empty());
  EXPECT_TRUE(chrome_path.value().rfind(kAlternateChromeExePath) !=
              std::string::npos);
  EXPECT_TRUE(base::PathExists(chrome_path));
  DeleteChromeExeRegistry();
  chrome_path = cloud_print::GetChromeExePath();
  // No Chrome or regular chrome path.
  EXPECT_TRUE(chrome_path.empty() ||
              chrome_path.value().rfind(kChromeExePath) == std::string::npos);
}

TEST_F(PortMonitorTest, GetPrintCommandTemplateTest) {
  std::wstring print_command = cloud_print::GetPrintCommandTemplate();
  EXPECT_FALSE(print_command.empty());
  EXPECT_EQ(print_command, kTestPrintCommand);
  DeleteChromeExeRegistry();
  print_command = cloud_print::GetPrintCommandTemplate();
  EXPECT_TRUE(print_command.empty());
}

TEST_F(PortMonitorTest, GetChromeProfilePathTest) {
  base::FilePath data_path = cloud_print::GetChromeProfilePath();
  EXPECT_FALSE(data_path.empty());
  base::FilePath temp;
  base::PathService::Get(base::DIR_TEMP, &temp);
  EXPECT_EQ(data_path, temp);
  EXPECT_TRUE(base::DirectoryExists(data_path));
  DeleteChromeExeRegistry();
  data_path = cloud_print::GetChromeProfilePath();
  EXPECT_TRUE(data_path.empty());
}

TEST_F(PortMonitorTest, EnumPortsTest) {
  DWORD needed_bytes = 0;
  DWORD returned = 0;
  EXPECT_FALSE(
      Monitor2EnumPorts(NULL, NULL, 1, NULL, 0, &needed_bytes, &returned));
  EXPECT_EQ(static_cast<DWORD>(ERROR_INSUFFICIENT_BUFFER), GetLastError());
  EXPECT_NE(0u, needed_bytes);
  EXPECT_EQ(0u, returned);

  BYTE* buffer = new BYTE[needed_bytes];
  ASSERT_TRUE(buffer != NULL);
  EXPECT_TRUE(Monitor2EnumPorts(NULL, NULL, 1, buffer, needed_bytes,
                                &needed_bytes, &returned));
  EXPECT_NE(0u, needed_bytes);
  EXPECT_EQ(1u, returned);
  PORT_INFO_1* port_info_1 = reinterpret_cast<PORT_INFO_1*>(buffer);
  EXPECT_TRUE(port_info_1->pName != NULL);
  delete[] buffer;

  returned = 0;
  needed_bytes = 0;
  EXPECT_FALSE(
      Monitor2EnumPorts(NULL, NULL, 2, NULL, 0, &needed_bytes, &returned));
  EXPECT_EQ(static_cast<DWORD>(ERROR_INSUFFICIENT_BUFFER), GetLastError());
  EXPECT_NE(0u, needed_bytes);
  EXPECT_EQ(0u, returned);

  buffer = new BYTE[needed_bytes];
  ASSERT_TRUE(buffer != NULL);
  EXPECT_TRUE(Monitor2EnumPorts(NULL, NULL, 2, buffer, needed_bytes,
                                &needed_bytes, &returned));
  EXPECT_NE(0u, needed_bytes);
  EXPECT_EQ(1u, returned);
  PORT_INFO_2* port_info_2 = reinterpret_cast<PORT_INFO_2*>(buffer);
  EXPECT_TRUE(port_info_2->pPortName != NULL);
  delete[] buffer;
}

TEST_F(PortMonitorTest, FlowTest) {
  const wchar_t kXcvDataItem[] = L"MonitorUI";
  MONITORINIT monitor_init = {0};
  HANDLE monitor_handle = NULL;
  HANDLE port_handle = NULL;
  HANDLE xcv_handle = NULL;
  DWORD bytes_processed = 0;
  DWORD bytes_needed = 0;
  const size_t kBufferSize = 100;
  BYTE buffer[kBufferSize] = {0};

  // Initialize the print monitor
  MONITOR2* monitor2 = InitializePrintMonitor2(&monitor_init, &monitor_handle);
  EXPECT_TRUE(monitor2 != NULL);
  EXPECT_TRUE(monitor_handle != NULL);

  // Test the XCV functions.  Used for reporting the location of the
  // UI portion of the port monitor.
  EXPECT_TRUE(monitor2->pfnXcvOpenPort != NULL);
  EXPECT_TRUE(monitor2->pfnXcvOpenPort(monitor_handle, NULL, 0, &xcv_handle));
  EXPECT_TRUE(xcv_handle != NULL);
  EXPECT_TRUE(monitor2->pfnXcvDataPort != NULL);
  EXPECT_EQ(static_cast<DWORD>(ERROR_ACCESS_DENIED),
            monitor2->pfnXcvDataPort(xcv_handle, kXcvDataItem, NULL, 0, buffer,
                                     kBufferSize, &bytes_needed));
  EXPECT_TRUE(monitor2->pfnXcvClosePort != NULL);
  EXPECT_TRUE(monitor2->pfnXcvClosePort(xcv_handle));
  EXPECT_TRUE(monitor2->pfnXcvOpenPort(monitor_handle, NULL,
                                       SERVER_ACCESS_ADMINISTER, &xcv_handle));
  EXPECT_TRUE(xcv_handle != NULL);
  EXPECT_TRUE(monitor2->pfnXcvDataPort != NULL);
  EXPECT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            monitor2->pfnXcvDataPort(xcv_handle, kXcvDataItem, NULL, 0, buffer,
                                     kBufferSize, &bytes_needed));
  EXPECT_TRUE(monitor2->pfnXcvClosePort != NULL);
  EXPECT_TRUE(monitor2->pfnXcvClosePort(xcv_handle));

  // Test opening the port and running a print job.
  EXPECT_TRUE(monitor2->pfnOpenPort != NULL);
  EXPECT_TRUE(monitor2->pfnOpenPort(monitor_handle, NULL, &port_handle));
  EXPECT_TRUE(port_handle != NULL);
  EXPECT_TRUE(monitor2->pfnStartDocPort != NULL);
  EXPECT_TRUE(monitor2->pfnWritePort != NULL);
  EXPECT_TRUE(monitor2->pfnReadPort != NULL);
  EXPECT_TRUE(monitor2->pfnEndDocPort != NULL);

  // These functions should fail if we have not impersonated the user.
  EXPECT_FALSE(monitor2->pfnStartDocPort(port_handle, const_cast<wchar_t*>(L""),
                                         0, 0, NULL));
  EXPECT_FALSE(monitor2->pfnWritePort(port_handle, buffer, kBufferSize,
                                      &bytes_processed));
  EXPECT_EQ(0u, bytes_processed);
  EXPECT_FALSE(monitor2->pfnReadPort(port_handle, buffer, sizeof(buffer),
                                     &bytes_processed));
  EXPECT_EQ(0u, bytes_processed);
  EXPECT_FALSE(monitor2->pfnEndDocPort(port_handle));

  // Now impersonate so we can test the success case.
  ASSERT_TRUE(ImpersonateSelf(SecurityImpersonation));
  EXPECT_TRUE(monitor2->pfnStartDocPort(port_handle, const_cast<wchar_t*>(L""),
                                        0, 0, NULL));
  EXPECT_TRUE(monitor2->pfnWritePort(port_handle, buffer, kBufferSize,
                                     &bytes_processed));
  EXPECT_EQ(kBufferSize, bytes_processed);
  EXPECT_FALSE(monitor2->pfnReadPort(port_handle, buffer, sizeof(buffer),
                                     &bytes_processed));
  EXPECT_EQ(0u, bytes_processed);
  EXPECT_TRUE(monitor2->pfnEndDocPort(port_handle));
  RevertToSelf();
  EXPECT_TRUE(monitor2->pfnClosePort != NULL);
  EXPECT_TRUE(monitor2->pfnClosePort(port_handle));
  // Shutdown the port monitor.
  Monitor2Shutdown(monitor_handle);
}

}  // namespace cloud_print
