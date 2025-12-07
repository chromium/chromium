// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_ELEVATED_TRACING_SERVICE_SERVICE_INTEGRATION_H_
#define CHROME_WINDOWS_SERVICES_ELEVATED_TRACING_SERVICE_SERVICE_INTEGRATION_H_

#include <string>

namespace elevated_tracing_service {

// Returns the basename of a directory created within C:\Windows\SystemTemp to
// hold persistent state such as crashpad databases.
std::wstring GetStorageDirBasename();

}  // namespace elevated_tracing_service

#endif  // CHROME_WINDOWS_SERVICES_ELEVATED_TRACING_SERVICE_SERVICE_INTEGRATION_H_
