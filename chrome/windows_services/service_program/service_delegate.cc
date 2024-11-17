// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/service_delegate.h"

#include "base/notreached.h"

uint16_t ServiceDelegate::GetLogEventCategory() {
  NOTREACHED();
}

uint32_t ServiceDelegate::GetLogEventMessageId() {
  NOTREACHED();
}

base::expected<base::HeapArray<FactoryAndClsid>, HRESULT>
ServiceDelegate::CreateClassFactories() {
  NOTREACHED();
  return base::unexpected(E_NOTIMPL);
}

bool ServiceDelegate::PreRun() {
  return false;  // This delegate does not implement `Run()`.
}

HRESULT ServiceDelegate::Run(const base::CommandLine& /*command_line*/) {
  NOTREACHED();
  return E_NOTIMPL;
}
