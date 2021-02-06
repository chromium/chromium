// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/elevation_service/service_main.h"

#include <wrl/client.h>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/install_static/install_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const base::FilePath TestFile(const std::string& file) {
  base::FilePath path;
  base::PathService::Get(base::DIR_MODULE, &path);
  return path.AppendASCII("elevated_recovery_unittest").AppendASCII(file);
}

}  // namespace

namespace elevation_service {

void SetupMockContext();
void TeardownMockContext();

}  // namespace elevation_service

class ServiceMainTest : public testing::Test {
 protected:
  ServiceMainTest() = default;

  void SetUp() override {
    com_initializer_ = std::make_unique<base::win::ScopedCOMInitializer>();
    ASSERT_TRUE(com_initializer_->Succeeded());

    service_main_ = elevation_service::ServiceMain::GetInstance();
    HRESULT hr = service_main_->RegisterClassObject();
    if (SUCCEEDED(hr))
      class_registration_succeeded_ = true;
    ASSERT_HRESULT_SUCCEEDED(hr);

    service_main_->ResetExitSignaled();
  }

  void TearDown() override {
    if (class_registration_succeeded_)
      service_main_->UnregisterClassObject();

    com_initializer_.reset();
  }

  elevation_service::ServiceMain* service_main() { return service_main_; }

 private:
  elevation_service::ServiceMain* service_main_ = nullptr;
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer_;
  bool class_registration_succeeded_ = false;

  DISALLOW_COPY_AND_ASSIGN(ServiceMainTest);
};

TEST_F(ServiceMainTest, ExitSignalTest) {
  // The waitable event starts unsignaled.
  ASSERT_FALSE(service_main()->IsExitSignaled());

  elevation_service::SetupMockContext();

  Microsoft::WRL::ComPtr<IUnknown> unknown;
  ASSERT_HRESULT_SUCCEEDED(
      ::CoCreateInstance(install_static::GetElevatorClsid(), nullptr,
                         CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&unknown)));

  Microsoft::WRL::ComPtr<IElevator> elevator;
  ASSERT_HRESULT_SUCCEEDED(unknown.As(&elevator));
  unknown.Reset();

  ULONG_PTR proc_handle = 0;
  EXPECT_EQ(CRYPT_E_NO_MATCH,
            elevator->RunRecoveryCRXElevated(
                TestFile("ChromeRecovery.crx3").value().c_str(),
                L"{c49ab053-2387-4809-b188-1902648802e1}", L"57.8.0.1",
                L"{c49ab053-2387-4809-b188-1902648802e1}",
                ::GetCurrentProcessId(), &proc_handle));

  // An object instance has been created upon the request, and is held by the
  // server module. Therefore, the waitable event remains unsignaled.
  ASSERT_FALSE(service_main()->IsExitSignaled());

  // Release the instance object. Now that the last (and the only) instance
  // object of the module is released, the event becomes signaled.
  elevator.Reset();
  elevation_service::TeardownMockContext();

  ASSERT_TRUE(service_main()->IsExitSignaled());
}
