// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_ELEVATED_TRACING_SERVICE_ELEVATED_TRACING_SERVICE_DELEGATE_H_
#define CHROME_WINDOWS_SERVICES_ELEVATED_TRACING_SERVICE_ELEVATED_TRACING_SERVICE_DELEGATE_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/windows_services/service_program/service_delegate.h"

namespace elevated_tracing_service {

class SessionRegistry;

class Delegate : public ServiceDelegate {
 public:
  Delegate();
  ~Delegate() override;

  uint16_t GetLogEventCategory() override;
  uint32_t GetLogEventMessageId() override;
  base::expected<base::HeapArray<FactoryAndClsid>, HRESULT>
  CreateClassFactories() override;
  void PreRun() override;
  void PostRun() override;

 private:
  scoped_refptr<SessionRegistry> session_registry_;
};

}  // namespace elevated_tracing_service

#endif  // CHROME_WINDOWS_SERVICES_ELEVATED_TRACING_SERVICE_ELEVATED_TRACING_SERVICE_DELEGATE_H_
