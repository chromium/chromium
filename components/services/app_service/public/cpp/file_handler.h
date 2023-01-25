// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_FILE_HANDLER_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_FILE_HANDLER_H_

#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "url/gurl.h"

namespace apps {

struct FileHandler {
  FileHandler();
  ~FileHandler();
  FileHandler(const FileHandler& file_handler);

  // Represents a single file handler "accept" entry, mapping a MIME type to a
  // set of file extensions that can be handled by the handler.
  struct AcceptEntry {
    AcceptEntry();
    ~AcceptEntry();
    AcceptEntry(const AcceptEntry& accept_entry);

    base::Value AsDebugValue() const;

    // A MIME type that can be handled by the file handler.
    std::string mime_type;

    // A set of one or more file extensions that can be handled by the file
    // handler, corresponding to the MIME type.
    base::flat_set<std::string> file_extensions;
  };

  base::Value AsDebugValue() const;

  // The URL that will be navigated to when dispatching on a file with a
  // matching MIME type or file extension.
  GURL action;

  // The user-visible name for the file type, e.g. "ACME Word Processor document
  // file". May be empty.
  std::u16string display_name;

  // A collection of MIME type to file extensions mappings that the handler
  // will match on.
  using Accept = std::vector<AcceptEntry>;
  Accept accept;

  // The icons defined for this file handler, to be used as file type
  // association icons in OS surfaces. The sizes in `downloaded_icons`, when
  // present, represent the actual size of a bitmap that was downloaded.
  std::vector<IconInfo> downloaded_icons;

  // How the app should be launched in the case where there are multiple files
  // being opened.
  enum class LaunchType {
    kSingleClient,
    kMultipleClients,
  };
  LaunchType launch_type = LaunchType::kSingleClient;
};
using FileHandlers = std::vector<FileHandler>;

// Get a set of all MIME types supported by any of |file_handlers|.
std::set<std::string> GetMimeTypesFromFileHandlers(
    const FileHandlers& file_handlers);

// Get a set of all MIME types supported by |file_handler|.
std::set<std::string> GetMimeTypesFromFileHandler(
    const FileHandler& file_handler);

// Get a set of all file extensions supported by any of |file_handlers|.
// Note: These are always transformed to lower-case.
std::set<std::string> GetFileExtensionsFromFileHandlers(
    const FileHandlers& file_handlers);

// Get a set of all file extensions supported by |file_handler|.
// Note: These are always transformed to lower-case.
std::set<std::string> GetFileExtensionsFromFileHandler(
    const FileHandler& file_handler);

bool operator==(const FileHandler::AcceptEntry& accept_entry1,
                const FileHandler::AcceptEntry& accept_entry2);
bool operator==(const FileHandler& file_handler1,
                const FileHandler& file_handler2);

bool operator!=(const FileHandler::AcceptEntry& accept_entry1,
                const FileHandler::AcceptEntry& accept_entry2);
bool operator!=(const FileHandler& file_handler1,
                const FileHandler& file_handler2);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_FILE_HANDLER_H_
