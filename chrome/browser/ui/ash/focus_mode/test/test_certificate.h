// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_FOCUS_MODE_TEST_TEST_CERTIFICATE_H_
#define CHROME_BROWSER_UI_ASH_FOCUS_MODE_TEST_TEST_CERTIFICATE_H_

#include <string>

namespace ash {

// Returns a DER encoded X.509 certificate self-signed with a SHA-1 signature
// and expires in 2124.
std::string ReadSha1TestCertificate();

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_FOCUS_MODE_TEST_TEST_CERTIFICATE_H_
