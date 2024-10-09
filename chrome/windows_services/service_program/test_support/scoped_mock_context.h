// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_TEST_SUPPORT_SCOPED_MOCK_CONTEXT_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_TEST_SUPPORT_SCOPED_MOCK_CONTEXT_H_

#include <unknwn.h>

#include <wrl/client.h>

#include "base/memory/raw_ptr.h"

// Installs an implementation of IServerSecurity that allows code under test to
// successfully call ::CoImpersonateClient().
class ScopedMockContext {
 public:
  ScopedMockContext();
  ScopedMockContext(const ScopedMockContext&) = delete;
  ScopedMockContext& operator=(const ScopedMockContext&) = delete;
  ~ScopedMockContext();

  bool Succeeded() const { return mock_call_context_; }

 private:
  Microsoft::WRL::ComPtr<IUnknown> mock_call_context_;
  raw_ptr<IUnknown> original_call_context_ = nullptr;
};

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_TEST_SUPPORT_SCOPED_MOCK_CONTEXT_H_
