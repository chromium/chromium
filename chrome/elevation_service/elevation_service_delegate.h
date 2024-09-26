// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELEVATION_SERVICE_ELEVATION_SERVICE_DELEGATE_H_
#define CHROME_ELEVATION_SERVICE_ELEVATION_SERVICE_DELEGATE_H_

#include "chrome/windows_services/service_program/service_delegate.h"

namespace elevation_service {

class Delegate : public ServiceDelegate {
 public:
  Delegate() = default;

  uint16_t GetLogEventCategory() override;
  uint32_t GetLogEventMessageId() override;
  base::expected<base::HeapArray<FactoryAndClsid>, HRESULT>
  CreateClassFactories() override;
  void PreRun() override;
};

}  // namespace elevation_service

#endif  // CHROME_ELEVATION_SERVICE_ELEVATION_SERVICE_DELEGATE_H_
