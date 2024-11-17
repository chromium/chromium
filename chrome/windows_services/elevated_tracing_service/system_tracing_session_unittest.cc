// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/elevated_tracing_service/system_tracing_session.h"

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/win_util.h"
#include "chrome/common/win/eventlog_messages.h"
#include "chrome/install_static/install_util.h"
#include "chrome/windows_services/elevated_tracing_service/elevated_tracing_service_delegate.h"
#include "chrome/windows_services/elevated_tracing_service/session_registry.h"
#include "chrome/windows_services/service_program/service.h"
#include "chrome/windows_services/service_program/test_support/scoped_mock_context.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DummyOuter
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUnknown> {
 public:
  using Microsoft::WRL::RuntimeClass<
      Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
      IUnknown>::CastToUnknown;
};

}  // namespace

class SystemTracingSessionTest : public ::testing::Test {
 protected:
  SystemTracingSessionTest() = default;

  void SetUp() override {
    ASSERT_TRUE(scoped_com_initializer_.Succeeded());
    ASSERT_HRESULT_SUCCEEDED(service_main_.RegisterClassObjects());
  }

  void TearDown() override { service_main_.UnregisterClassObjects(); }

  elevated_tracing_service::Delegate& service_delegate() {
    return service_delegate_;
  }
  Service& service_main() { return service_main_; }

 private:
  base::test::TaskEnvironment task_environment_;
  base::win::ScopedCOMInitializer scoped_com_initializer_;
  scoped_refptr<elevated_tracing_service::SessionRegistry> session_registry_{
      base::MakeRefCounted<elevated_tracing_service::SessionRegistry>()};
  elevated_tracing_service::Delegate service_delegate_;
  Service service_main_{service_delegate_};
};

TEST_F(SystemTracingSessionTest, GetLogEventCategory) {
  ASSERT_EQ(service_delegate().GetLogEventCategory(), TRACING_SERVICE_CATEGORY);
}

TEST_F(SystemTracingSessionTest, GetLogEventMessageId) {
  ASSERT_EQ(service_delegate().GetLogEventMessageId(),
            MSG_TRACING_SERVICE_LOG_MESSAGE);
}

TEST_F(SystemTracingSessionTest, NoAggregation) {
  ScopedMockContext mock_context;
  ASSERT_TRUE(mock_context.Succeeded());

  auto dummy_outer = Microsoft::WRL::Make<DummyOuter>();
  ASSERT_TRUE(bool(dummy_outer));

  Microsoft::WRL::ComPtr<IUnknown> unknown;
  ASSERT_EQ(::CoCreateInstance(install_static::GetTracingServiceClsid(),
                               dummy_outer->CastToUnknown(),
                               CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&unknown)),
            CLASS_E_NOAGGREGATION);
}

// Tests that AcceptInvitation returns E_INVALIDARG as appropriate.
TEST_F(SystemTracingSessionTest, AcceptInvitation) {
  ScopedMockContext mock_context;
  ASSERT_TRUE(mock_context.Succeeded());

  Microsoft::WRL::ComPtr<IUnknown> unknown;
  ASSERT_HRESULT_SUCCEEDED(
      ::CoCreateInstance(install_static::GetTracingServiceClsid(), nullptr,
                         CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&unknown)));

  Microsoft::WRL::ComPtr<ISystemTraceSession> trace_session;
  ASSERT_HRESULT_SUCCEEDED(unknown.As(&trace_session));
  unknown.Reset();

  DWORD pid = base::kNullProcessId;
  ASSERT_EQ(trace_session->AcceptInvitation(nullptr, &pid), E_INVALIDARG);
  ASSERT_EQ(trace_session->AcceptInvitation(L"", &pid), E_INVALIDARG);
  ASSERT_EQ(trace_session->AcceptInvitation(L"invalid", nullptr), E_INVALIDARG);
}
