// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_IOS_METRICS_UTIL_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_IOS_METRICS_UTIL_H_

#include <string>

#include "components/enterprise/client_certificates/ios/client_identity_ios_error.h"

namespace client_certificates {

// Logs a terminal `error` that occurred when trying to instantiate a
// `ClientIdentityIOS` instance. The `logging_context` is used to differentiate
// between "Browser" and "Profile" levels.
void LogClientIdentityIOSError(const std::string& logging_context,
                               ClientIdentityIOSError error);

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_IOS_METRICS_UTIL_H_
