// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_UPLOAD_CLIENT_ERROR_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_UPLOAD_CLIENT_ERROR_H_

#include "base/types/expected.h"

namespace client_certificates {

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

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_UPLOAD_CLIENT_ERROR_H_
