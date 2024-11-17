// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SERVICE_PROGRAM_MAIN_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SERVICE_PROGRAM_MAIN_H_

class ServiceDelegate;

// The main program entrypoint for a Windows service. Service executables must
// call this from their wWinMain function, providing it with a custom delegate.
// Returns the service's process exit code.
int ServiceProgramMain(ServiceDelegate& delegate);

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SERVICE_PROGRAM_MAIN_H_
