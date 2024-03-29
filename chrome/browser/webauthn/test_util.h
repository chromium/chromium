// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_TEST_UTIL_H_
#define CHROME_BROWSER_WEBAUTHN_TEST_UTIL_H_

#include <utility>

#include "base/files/file_path.h"
#include "base/process/process.h"
#include "device/fido/enclave/constants.h"

// Starts a test enclave service instance. Returns the Process instance and
// the port number on which the enclave is listening for connections.
// CHECKs on error.
std::pair<base::Process, uint16_t> StartWebAuthnEnclave(base::FilePath cwd);

device::enclave::ScopedEnclaveOverride TestWebAuthnEnclaveIdentity(
    uint16_t port);

#endif  // CHROME_BROWSER_WEBAUTHN_TEST_UTIL_H_
