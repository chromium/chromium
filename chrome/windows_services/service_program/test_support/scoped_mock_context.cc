// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/test_support/scoped_mock_context.h"

#include <objbase.h>

#include <unknwn.h>

#include <objidl.h>
#include <wrl/implements.h>

#include "base/win/com_init_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A mock implementation of IServerSecurity that allows for the production code
// that calls ::CoImpersonateClient() to work.
class MockServerSecurity
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IServerSecurity> {
 public:
  MockServerSecurity() = default;
  MockServerSecurity(const MockServerSecurity&) = delete;
  MockServerSecurity& operator=(const MockServerSecurity&) = delete;

  IFACEMETHODIMP QueryBlanket(DWORD* authentication_service,
                              DWORD* authorization_service,
                              OLECHAR** server_principal_name,
                              DWORD* authentication_level,
                              DWORD* impersonation_level,
                              void** privilege,
                              DWORD* capabilities) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP ImpersonateClient() override {
    is_impersonating_ = true;
    return S_OK;
  }
  IFACEMETHODIMP RevertToSelf() override {
    is_impersonating_ = false;
    return S_OK;
  }
  IFACEMETHODIMP_(BOOL) IsImpersonating() override { return is_impersonating_; }

 private:
  ~MockServerSecurity() override { EXPECT_FALSE(is_impersonating_); }

  bool is_impersonating_ = false;
};

}  // namespace

ScopedMockContext::ScopedMockContext() {
  base::win::AssertComInitialized();
  auto mock_call_context = Microsoft::WRL::Make<MockServerSecurity>();

  // We set the call context to a mock object that implements IServerSecurity.
  // This allows for the production code that calls ::CoImpersonateClient() to
  // succeed.
  auto hresult = ::CoSwitchCallContext(
      mock_call_context.Get(), &original_call_context_.AsEphemeralRawAddr());
  EXPECT_HRESULT_SUCCEEDED(hresult);
  if (FAILED(hresult)) {
    return;
  }

  mock_call_context_ = std::move(mock_call_context);
  EXPECT_EQ(original_call_context_, nullptr);
}

ScopedMockContext::~ScopedMockContext() {
  base::win::AssertComInitialized();
  if (!Succeeded()) {
    return;
  }

  IUnknown* this_call_context = nullptr;
  EXPECT_HRESULT_SUCCEEDED(
      ::CoSwitchCallContext(original_call_context_.get(), &this_call_context));
  EXPECT_EQ(this_call_context, mock_call_context_.Get())
      << "CoSwitchCallContext switched out someone else's context.";
}
