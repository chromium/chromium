// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/file_handler.h"

#include <tuple>

#include "base/strings/string_util.h"

namespace apps {

FileHandler::FileHandler() = default;
FileHandler::~FileHandler() = default;
FileHandler::FileHandler(const FileHandler& file_handler) = default;

FileHandler::AcceptEntry::AcceptEntry() = default;
FileHandler::AcceptEntry::~AcceptEntry() = default;
FileHandler::AcceptEntry::AcceptEntry(const AcceptEntry& accept_entry) =
    default;

base::Value FileHandler::AcceptEntry::AsDebugValue() const {
  base::Value::Dict root;

  root.Set("mime_type", mime_type);
  base::Value::List& file_extensions_json = *root.EnsureList("file_extensions");
  for (const std::string& file_extension : file_extensions)
    file_extensions_json.Append(file_extension);

  return base::Value(std::move(root));
}

base::Value FileHandler::AsDebugValue() const {
  base::Value::Dict root;

  base::Value::List& accept_json = *root.EnsureList("accept");
  for (const AcceptEntry& entry : accept)
    accept_json.Append(entry.AsDebugValue());
  root.Set("action", action.spec());
  base::Value::List& icons_json = *root.EnsureList("downloaded_icons");
  for (const IconInfo& entry : downloaded_icons)
    icons_json.Append(entry.AsDebugValue());
  root.Set("name", display_name);
  root.Set("launch_type", launch_type == LaunchType::kSingleClient
                              ? "kSingleClient"
                              : "kMultipleClients");

  return base::Value(std::move(root));
}

std::set<std::string> GetMimeTypesFromFileHandlers(
    const FileHandlers& file_handlers) {
  std::set<std::string> mime_types;
  for (const auto& file_handler : file_handlers) {
    std::set<std::string> file_handler_mime_types =
        GetMimeTypesFromFileHandler(file_handler);
    mime_types.insert(file_handler_mime_types.begin(),
                      file_handler_mime_types.end());
  }
  return mime_types;
}

std::set<std::string> GetMimeTypesFromFileHandler(
    const FileHandler& file_handler) {
  std::set<std::string> mime_types;
  for (const auto& accept_entry : file_handler.accept)
    mime_types.insert(accept_entry.mime_type);
  return mime_types;
}

std::set<std::string> GetFileExtensionsFromFileHandlers(
    const FileHandlers& file_handlers) {
  std::set<std::string> file_extensions;
  for (const auto& file_handler : file_handlers) {
    std::set<std::string> file_handler_file_extensions =
        GetFileExtensionsFromFileHandler(file_handler);
    file_extensions.insert(file_handler_file_extensions.begin(),
                           file_handler_file_extensions.end());
  }
  return file_extensions;
}

std::set<std::string> GetFileExtensionsFromFileHandler(
    const FileHandler& file_handler) {
  std::set<std::string> file_extensions;
  for (const auto& accept_entry : file_handler.accept) {
    for (const std::string& extension : accept_entry.file_extensions) {
      file_extensions.insert(base::ToLowerASCII(extension));
    }
  }
  return file_extensions;
}

bool operator==(const FileHandler::AcceptEntry& accept_entry1,
                const FileHandler::AcceptEntry& accept_entry2) {
  return std::tie(accept_entry1.mime_type, accept_entry1.file_extensions) ==
         std::tie(accept_entry2.mime_type, accept_entry2.file_extensions);
}

bool operator==(const FileHandler& file_handler1,
                const FileHandler& file_handler2) {
  return std::tie(file_handler1.action, file_handler1.accept,
                  file_handler1.display_name, file_handler1.launch_type) ==
         std::tie(file_handler2.action, file_handler2.accept,
                  file_handler2.display_name, file_handler2.launch_type);
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
