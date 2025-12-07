// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/extension/service.h"

#include <windows.h>

#include "base/files/file_path.h"
#include "base/threading/thread.h"
#include "chrome/credential_provider/extension/scoped_handle.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

class GCPWServiceTest : public ::testing::Test {
 public:
  // Start the service as if SCM is starting it. Note that this will be started
  // in a new thread which simulates the main thread when service process
  // starts.
  void StartServiceMainThread() {
    main_thread->Start();
    main_thread->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&GCPWServiceTest::RunService, base::Unretained(this)));
    // Give some time so that service starts running and accpeting control
    // requests.
    ::Sleep(1000);
  }

  void CheckServiceStatus(DWORD status) {
    SERVICE_STATUS service_status;
    fake_os_service_manager_.GetServiceStatus(&service_status);
    ASSERT_EQ(service_status.dwCurrentState, status);
  }

  void SendControlRequest(DWORD control_request) {
    fake_os_service_manager_.SendControlRequestForTesting(control_request);
    // Give some time for control to be processed by the service control
    // handler.
    ::Sleep(1000);
  }

  FakeOSServiceManager* fake_os_service_manager() {
    return &fake_os_service_manager_;
  }

 protected:
  void SetUp() override {
    main_thread = std::make_unique<base::Thread>("ProcessMain Thread");
  }

  void TearDown() override {
    SendControlRequest(SERVICE_CONTROL_STOP);
    main_thread->Stop();
  }

 private:
  void RunService() {
    ASSERT_TRUE(credential_provider::extension::Service::Get()->Run() ==
                ERROR_SUCCESS);
  }

  std::unique_ptr<base::Thread> main_thread;
  FakeOSServiceManager fake_os_service_manager_;
};

TEST_F(GCPWServiceTest, StartSuccess) {
  // Install service into SCM database.
  credential_provider::extension::ScopedScHandle sc_handle;
  ASSERT_TRUE(ERROR_SUCCESS == fake_os_service_manager()->InstallService(
                                   base::FilePath(L"test"), &sc_handle));

  StartServiceMainThread();
  CheckServiceStatus(SERVICE_RUNNING);

  SendControlRequest(SERVICE_CONTROL_STOP);
  CheckServiceStatus(SERVICE_STOPPED);
}

TEST_F(GCPWServiceTest, NoOpControlRequest) {
  // Install service into SCM database.
  credential_provider::extension::ScopedScHandle sc_handle;
  ASSERT_TRUE(ERROR_SUCCESS == fake_os_service_manager()->InstallService(
                                   base::FilePath(L"test"), &sc_handle));

  StartServiceMainThread();
  CheckServiceStatus(SERVICE_RUNNING);

  SendControlRequest(SERVICE_CONTROL_PAUSE);
  CheckServiceStatus(SERVICE_RUNNING);
}

}  // namespace testing
}  // namespace credential_provider
