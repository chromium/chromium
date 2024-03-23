// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/elevation_service/service_main.h"

#include <wrl/client.h>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/elevation_service/scoped_mock_context.h"
#include "chrome/install_static/install_util.h"
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

  void SetUp() override {
    ASSERT_TRUE(com_initializer_.Succeeded());

    auto* service_main = elevation_service::ServiceMain::GetInstance();
    ASSERT_HRESULT_SUCCEEDED(service_main->RegisterClassObject());
    service_main_ = service_main;
    service_main_->ResetExitSignaled();
  }

  void TearDown() override {
    if (service_main_)
      std::exchange(service_main_, nullptr)->UnregisterClassObject();
  }

  elevation_service::ServiceMain* service_main() { return service_main_; }

 private:
  base::win::ScopedCOMInitializer com_initializer_;
  raw_ptr<elevation_service::ServiceMain> service_main_ = nullptr;
};

TEST_F(ServiceMainTest, ExitSignalTest) {
  // The waitable event starts unsignaled.
  ASSERT_FALSE(service_main()->IsExitSignaled());

  {
    elevation_service::ScopedMockContext mock_context;
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
    // server module. Therefore, the waitable event remains unsignaled.
    ASSERT_FALSE(service_main()->IsExitSignaled());

    // Release the instance object. Now that the last (and the only) instance
    // object of the module is released, the event becomes signaled.
    elevator.Reset();
  }

  ASSERT_TRUE(service_main()->IsExitSignaled());
}

TEST_F(ServiceMainTest, EncryptDecryptTest) {
  elevation_service::ScopedMockContext mock_context;
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
