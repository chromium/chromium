// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_ELEVATED_TRACING_SERVICE_SYSTEM_TRACING_SESSION_H_
#define CHROME_WINDOWS_SERVICES_ELEVATED_TRACING_SERVICE_SYSTEM_TRACING_SESSION_H_

#include <windows.h>

#include <wrl/implements.h>

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "chrome/windows_services/elevated_tracing_service/session_registry.h"
#include "components/tracing/tracing_service_idl.h"

namespace base {
class SequencedTaskRunner;
}

namespace elevated_tracing_service {

inline constexpr IID kTestSystemTracingSessionClsid = {
    0xB8879CA1,
    0x6E13,
    0x4137,
    {0xB7, 0xFA, 0x8D, 0xB1, 0x60, 0xA6, 0x10,
     0xB3}};  // SystemTracingSession Test CLSID.
              // {B8879CA1-6E13-4137-B7FA-8DB160A610B3}

namespace switches {
inline constexpr char kSystemTracingClsIdForTestingSwitch[] =
    "system-tracing-clsid-for-testing";
}  // namespace switches

class SystemTracingSession
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ISystemTraceSession,
          ISystemTraceSessionChromium,
          ISystemTraceSessionChrome,
          ISystemTraceSessionChromeBeta,
          ISystemTraceSessionChromeDev,
          ISystemTraceSessionChromeCanary> {
 public:
  SystemTracingSession();
  SystemTracingSession(const SystemTracingSession&) = delete;
  SystemTracingSession& operator=(const SystemTracingSession&) = delete;

  HRESULT RuntimeClassInitialize(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // ISystemTraceSession:
  IFACEMETHODIMP AcceptInvitation(const wchar_t* channel_name,
                                  DWORD* pid) override;

 private:
  ~SystemTracingSession() override;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<SessionRegistry::ScopedSession> session_;
};

}  // namespace elevated_tracing_service

#endif  // CHROME_WINDOWS_SERVICES_ELEVATED_TRACING_SERVICE_SYSTEM_TRACING_SESSION_H_
