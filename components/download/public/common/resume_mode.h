// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_RESUME_MODE_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_RESUME_MODE_H_

// The means by which the download was resumed.
// Used by DownloadItemImpl and UKM metrics.
namespace download {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.download
enum class ResumeMode {
  INVALID = 0,
  IMMEDIATE_CONTINUE,
  IMMEDIATE_RESTART,
  USER_CONTINUE,
  USER_RESTART
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_RESUME_MODE_H_
