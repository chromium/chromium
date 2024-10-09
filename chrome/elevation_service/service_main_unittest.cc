// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wrl/client.h>

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/mock_callback.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/elevation_service/elevation_service_delegate.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/install_static/install_util.h"
#include "chrome/windows_services/service_program/process_wrl_module.h"
#include "chrome/windows_services/service_program/service.h"
#include "chrome/windows_services/service_program/test_support/scoped_mock_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const base::FilePath TestFile(const std::string& file) {
  base::FilePath path;
  base::PathService::Get(base::DIR_MODULE, &path);
  return path.AppendASCII("elevated_recovery_unittest").AppendASCII(file);
}

}  // namespace

class ServiceMainTest : public testing::Test {
 protected:
  ServiceMainTest() = default;
  ~ServiceMainTest() override {
    SetModuleReleasedCallback({});
    service_main_.UnregisterClassObjects();
  }

  void SetUp() override {
    ASSERT_TRUE(com_initializer_.Succeeded());

    ASSERT_HRESULT_SUCCEEDED(service_main_.RegisterClassObjects());
  }

  Service& service_main() { return service_main_; }

 private:
  base::win::ScopedCOMInitializer com_initializer_;
  elevation_service::Delegate service_delegate_;
  Service service_main_{service_delegate_};
};

TEST_F(ServiceMainTest, ExitSignalTest) {
  ::testing::StrictMock<base::MockCallback<base::OnceClosure>>
      module_released_callback;
  SetModuleReleasedCallback(module_released_callback.Get());

  {
    ScopedMockContext mock_context;
    ASSERT_TRUE(mock_context.Succeeded());

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
    // server module. Therefore, the callback has not yet run.
    ::testing::Mock::VerifyAndClearExpectations(&module_released_callback);

    // Release the instance object. Now that the last (and the only) instance
    // object of the module is released, the event becomes signaled.
    EXPECT_CALL(module_released_callback, Run());
    elevator.Reset();
  }
}

TEST_F(ServiceMainTest, EncryptDecryptTest) {
  ScopedMockContext mock_context;
  ASSERT_TRUE(mock_context.Succeeded());

  Microsoft::WRL::ComPtr<IUnknown> unknown;
  ASSERT_HRESULT_SUCCEEDED(
      ::CoCreateInstance(install_static::GetElevatorClsid(), nullptr,
                         CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&unknown)));

  Microsoft::WRL::ComPtr<IElevator> elevator;
  ASSERT_HRESULT_SUCCEEDED(unknown.As(&elevator));
  unknown.Reset();

  std::string plaintext("hello world");
  BSTR input = ::SysAllocStringByteLen(nullptr, plaintext.length());
  ASSERT_TRUE(input);

  memcpy(input, plaintext.data(), plaintext.length());
  base::win::ScopedBstr output;
  DWORD last_error;
  HRESULT hr =
      elevator->EncryptData(ProtectionLevel::PROTECTION_PATH_VALIDATION, input,
                            output.Receive(), &last_error);
  ::SysFreeString(input);

  ASSERT_HRESULT_SUCCEEDED(hr);

  std::string encrypted;
  encrypted.assign(reinterpret_cast<const char*>(output.Get()),
                   output.ByteLength());

  BSTR input2 = ::SysAllocStringByteLen(nullptr, encrypted.length());
  memcpy(input2, encrypted.data(), encrypted.length());
  base::win::ScopedBstr original;

  hr = elevator->DecryptData(input2, original.Receive(), &last_error);

  ::SysFreeString(input);

  ASSERT_HRESULT_SUCCEEDED(hr);

  elevator.Reset();

  std::string original_string;
  original_string.assign(reinterpret_cast<const char*>(original.Get()),
                         original.ByteLength());

  ASSERT_EQ(plaintext.length(), original_string.length());
  ASSERT_EQ(plaintext, original_string);
}
