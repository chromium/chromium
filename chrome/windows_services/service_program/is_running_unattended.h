// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_IS_RUNNING_UNATTENDED_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_IS_RUNNING_UNATTENDED_H_

namespace internal {

// Returns true if the service is running on behalf of a test in unattended
// mode.
bool IsRunningUnattended();

}  // namespace internal

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_IS_RUNNING_UNATTENDED_H_
