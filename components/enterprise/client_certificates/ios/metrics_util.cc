// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/ios/metrics_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"

namespace client_certificates {

void LogClientIdentityIOSError(const std::string& logging_context,
                               ClientIdentityIOSError error) {
  static constexpr char kClientIdentityErrorHistogram[] =
      "Enterprise.ClientCertificate.%s.IOS.ClientIdentityError";
  base::UmaHistogramEnumeration(
      base::StringPrintf(kClientIdentityErrorHistogram,
                         logging_context.c_str()),
      error);
}

}  // namespace client_certificates
