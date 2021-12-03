// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_FILE_HANDLERS_PERMISSION_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_FILE_HANDLERS_PERMISSION_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/web_app_id.h"

struct WebApplicationInfo;

namespace web_app {

class WebAppInstallFinalizer;
class WebApp;
enum class FileHandlerUpdateAction;

// TODO(crbug.com/1272000): remove this class, which no longer contains
// sufficient functionality to warrant independence.
class FileHandlersPermissionHelper {
 public:
  explicit FileHandlersPermissionHelper(WebAppInstallFinalizer* finalizer);
  ~FileHandlersPermissionHelper() = default;
  FileHandlersPermissionHelper(const FileHandlersPermissionHelper& other) =
      delete;
  FileHandlersPermissionHelper& operator=(
      const FileHandlersPermissionHelper& other) = delete;

  // To be called before `web_app` is updated with `web_app_info` changes.
  // Checks whether OS registered file handlers need to update, taking into
  // account permission settings, as file handlers should be unregistered when
  // the permission has been denied. Also, downgrades granted file handling
  // permissions if file handlers have changed. Returns whether the OS file
  // handling registrations need to be updated.
  FileHandlerUpdateAction WillUpdateApp(const WebApp& web_app,
                                        const WebApplicationInfo& web_app_info);

 private:
  raw_ptr<WebAppInstallFinalizer> finalizer_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_FILE_HANDLERS_PERMISSION_HELPER_H_
