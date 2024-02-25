// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_TASK_DOWNLOAD_TASK_TYPES_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_TASK_DOWNLOAD_TASK_TYPES_H_

namespace download {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.download
enum class DownloadTaskType {
  // Task to invoke download service to take various download actions.
  DOWNLOAD_TASK = 0,

  // Task to remove unnecessary files from the system.
  CLEANUP_TASK = 1,

  // Task to invoke the download auto-resumption handler.
  DOWNLOAD_AUTO_RESUMPTION_TASK = 2,

  // Task to start user scheduled downloads.
  DOWNLOAD_LATER_TASK = 3,

  // Task to invoke the download auto-resumption handler.
  DOWNLOAD_AUTO_RESUMPTION_UNMETERED_TASK = 4,

  // Task to invoke the download auto-resumption handler.
  DOWNLOAD_AUTO_RESUMPTION_ANY_NETWORK_TASK = 5,
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_TASK_DOWNLOAD_TASK_TYPES_H_
