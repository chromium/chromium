// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_UPLOAD_CLIENT_ERROR_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_UPLOAD_CLIENT_ERROR_H_

#include <string_view>

#include "base/types/expected.h"

namespace client_certificates {

// Captures terminal client-failure states (happening before even trying to
// upload) of the upload client flows. Do not reorder values as they are used in
// histograms logging (CertificateUploadClientError in enums.xml).
enum class UploadClientError {
  kUnknown = 0,
  kInvalidKeyParameter = 1,
  kSignatureCreationFailed = 2,
  kMissingDMToken = 3,
  kMissingUploadURL = 4,
  kInvalidUploadURL = 5,
  kMaxValue = kInvalidUploadURL
};

using HttpCodeOrClientError = base::expected<int, UploadClientError>;

template <class T>
using UploadClientErrorOr = base::expected<T, UploadClientError>;

std::string_view UploadClientErrorToString(UploadClientError error);

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_UPLOAD_CLIENT_ERROR_H_
