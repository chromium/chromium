// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_RESUMABLE_UPLOADER_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_RESUMABLE_UPLOADER_H_

#include "components/enterprise/connectors/core/cloud_content_scanning/resumable_uploader_base.h"

namespace enterprise_connectors {

using ResumableUploadRequest = ResumableUploadRequestBase;

}  // namespace enterprise_connectors

namespace safe_browsing {

// TODO(crbug.com/481674868): Combine resumable uploader base class with this.
using ResumableUploadRequest =
    ::enterprise_connectors::ResumableUploadRequestBase;

}  // namespace safe_browsing

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_RESUMABLE_UPLOADER_H_
