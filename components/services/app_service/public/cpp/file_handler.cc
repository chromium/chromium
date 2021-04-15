// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/file_handler.h"

#include <tuple>

namespace apps {

FileHandler::FileHandler() = default;
FileHandler::~FileHandler() = default;
FileHandler::FileHandler(const FileHandler& file_handler) = default;

FileHandler::AcceptEntry::AcceptEntry() = default;
FileHandler::AcceptEntry::~AcceptEntry() = default;
FileHandler::AcceptEntry::AcceptEntry(const AcceptEntry& accept_entry) =
    default;

std::set<std::string> GetMimeTypesFromFileHandlers(
    const FileHandlers& file_handlers) {
  std::set<std::string> mime_types;
  for (const auto& file_handler : file_handlers) {
    for (const auto& accept_entry : file_handler.accept)
      mime_types.insert(accept_entry.mime_type);
  }
  return mime_types;
}

std::set<std::string> GetFileExtensionsFromFileHandlers(
    const FileHandlers& file_handlers) {
  std::set<std::string> file_extensions;
  for (const auto& file_handler : file_handlers) {
    for (const auto& accept_entry : file_handler.accept)
      file_extensions.insert(accept_entry.file_extensions.begin(),
                             accept_entry.file_extensions.end());
  }
  return file_extensions;
}

std::ostream& operator<<(std::ostream& out,
                         const FileHandler::AcceptEntry& accept_entry) {
  out << "    mime_type: " << accept_entry.mime_type << std::endl;
  out << "    file_extensions:";
  for (const auto& file_extension : accept_entry.file_extensions)
    out << " " << file_extension;
  out << std::endl;
  return out;
}

std::ostream& operator<<(std::ostream& out, const FileHandler& file_handler) {
  out << "action: " << file_handler.action << std::endl;
  for (const auto& accept_entry : file_handler.accept) {
    out << "  accept:" << std::endl;
    out << accept_entry;
  }
  return out;
}

bool operator==(const FileHandler::AcceptEntry& accept_entry1,
                const FileHandler::AcceptEntry& accept_entry2) {
  return std::tie(accept_entry1.mime_type, accept_entry1.file_extensions) ==
         std::tie(accept_entry2.mime_type, accept_entry2.file_extensions);
}

bool operator==(const FileHandler& file_handler1,
                const FileHandler& file_handler2) {
  return std::tie(file_handler1.action, file_handler1.accept) ==
         std::tie(file_handler2.action, file_handler2.accept);
}

bool operator!=(const FileHandler::AcceptEntry& accept_entry1,
                const FileHandler::AcceptEntry& accept_entry2) {
  return !(accept_entry1 == accept_entry2);
}

bool operator!=(const FileHandler& file_handler1,
                const FileHandler& file_handler2) {
  return !(file_handler1 == file_handler2);
}

}  // namespace apps
