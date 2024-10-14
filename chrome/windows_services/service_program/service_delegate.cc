// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/service_delegate.h"

#include "base/notreached.h"

bool ServiceDelegate::PreRun() {
  return false;
}

HRESULT ServiceDelegate::Run(const base::CommandLine& /*command_line*/) {
  NOTREACHED();
  return E_NOTIMPL;
}
