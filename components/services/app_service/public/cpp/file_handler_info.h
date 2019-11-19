// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_FILE_HANDLER_INFO_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_FILE_HANDLER_INFO_H_

#include <set>
#include <string>

namespace apps {

namespace file_handler_verbs {

// Supported verbs for file handlers.
extern const char kOpenWith[];
extern const char kAddTo[];
extern const char kPackWith[];
extern const char kShareWith[];

}  // namespace file_handler_verbs

// Contains information about a file handler for an app.
struct FileHandlerInfo {
  FileHandlerInfo();
  FileHandlerInfo(const FileHandlerInfo& other);
  ~FileHandlerInfo();

  // The id of this handler.
  std::string id;

  // File extensions associated with this handler.
  std::set<std::string> extensions;

  // MIME types associated with this handler.
  std::set<std::string> types;

  // True if the handler can manage directories.
  bool include_directories;

  // A verb describing the intent of the handler.
  std::string verb;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_FILE_HANDLER_INFO_H_