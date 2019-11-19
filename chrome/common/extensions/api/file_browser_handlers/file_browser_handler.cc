// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"

#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/url_pattern.h"
#include "url/url_constants.h"

namespace keys = extensions::manifest_keys;
namespace errors = extensions::manifest_errors;

namespace {

const char kReadAccessString[] = "read";
const char kReadWriteAccessString[] = "read-write";
const char kCreateAccessString[] = "create";

unsigned int kPermissionsNotDefined = 0;
unsigned int kReadPermission = 1;
unsigned int kWritePermission = 1 << 1;
unsigned int kCreatePermission = 1 << 2;
unsigned int kInvalidPermission = 1 << 3;

unsigned int GetAccessPermissionFlagFromString(const std::string& access_str) {
  if (access_str == kReadAccessString)
    return kReadPermission;
  if (access_str == kReadWriteAccessString)
    return kReadPermission | kWritePermission;
  if (access_str == kCreateAccessString)
    return kCreatePermission;
  return kInvalidPermission;
}

// Stored on the Extension.
struct FileBrowserHandlerInfo : public extensions::Extension::ManifestData {
  FileBrowserHandler::List file_browser_handlers;

  FileBrowserHandlerInfo();
  ~FileBrowserHandlerInfo() override;
};

FileBrowserHandlerInfo::FileBrowserHandlerInfo() {
}

FileBrowserHandlerInfo::~FileBrowserHandlerInfo() {
}

}  // namespace

FileBrowserHandler::FileBrowserHandler()
    : file_access_permission_flags_(kPermissionsNotDefined) {
}

FileBrowserHandler::~FileBrowserHandler() {
}

void FileBrowserHandler::AddPattern(const URLPattern& pattern) {
  url_set_.AddPattern(pattern);
}

void FileBrowserHandler::ClearPatterns() {
  url_set_.ClearPatterns();
}

bool FileBrowserHandler::MatchesURL(const GURL& url) const {
  return url_set_.MatchesURL(url);
}

bool FileBrowserHandler::AddFileAccessPermission(
    const std::string& access) {
  file_access_permission_flags_ |= GetAccessPermissionFlagFromString(access);
  return (file_access_permission_flags_ & kInvalidPermission) != 0U;
}

bool FileBrowserHandler::ValidateFileAccessPermissions() {
  bool is_invalid = (file_access_permission_flags_ & kInvalidPermission) != 0U;
  bool can_create = (file_access_permission_flags_ & kCreatePermission) != 0U;
  bool can_read_or_write = (file_access_permission_flags_ &
      (kReadPermission | kWritePermission)) != 0U;
  if (is_invalid || (can_create && can_read_or_write)) {
    file_access_permission_flags_ = kInvalidPermission;
    return false;
  }

  if (file_access_permission_flags_ == kPermissionsNotDefined)
    file_access_permission_flags_ = kReadPermission | kWritePermission;
  return true;
}

bool FileBrowserHandler::CanRead() const {
  DCHECK(!(file_access_permission_flags_ & kInvalidPermission));
  return (file_access_permission_flags_ & kReadPermission) != 0;
}

bool FileBrowserHandler::CanWrite() const {
  DCHECK(!(file_access_permission_flags_ & kInvalidPermission));
  return (file_access_permission_flags_ & kWritePermission) != 0;
}

bool FileBrowserHandler::HasCreateAccessPermission() const {
  DCHECK(!(file_access_permission_flags_ & kInvalidPermission));
  return (file_access_permission_flags_ & kCreatePermission) != 0;
}

// static
FileBrowserHandler::List*
FileBrowserHandler::GetHandlers(const extensions::Extension* extension) {
  FileBrowserHandlerInfo* const info = static_cast<FileBrowserHandlerInfo*>(
      extension->GetManifestData(keys::kFileBrowserHandlers));
  if (!info)
    return nullptr;

  return &info->file_browser_handlers;
}

FileBrowserHandlerParser::FileBrowserHandlerParser() {
}

FileBrowserHandlerParser::~FileBrowserHandlerParser() {
}

namespace {

std::unique_ptr<FileBrowserHandler> LoadFileBrowserHandler(
    const std::string& extension_id,
    const base::DictionaryValue* file_browser_handler,
    base::string16* error) {
  std::unique_ptr<FileBrowserHandler> result(new FileBrowserHandler());
  result->set_extension_id(extension_id);

  std::string handler_id;
  // Read the file action |id| (mandatory).
  if (!file_browser_handler->HasKey(keys::kFileBrowserHandlerId) ||
      !file_browser_handler->GetString(keys::kFileBrowserHandlerId,
                                       &handler_id)) {
    *error = base::ASCIIToUTF16(errors::kInvalidFileBrowserHandlerId);
    return nullptr;
  }
  result->set_id(handler_id);

  // Read the page action title from |default_title| (mandatory).
  std::string title;
  if (!file_browser_handler->HasKey(keys::kActionDefaultTitle) ||
      !file_browser_handler->GetString(keys::kActionDefaultTitle, &title)) {
    *error = base::ASCIIToUTF16(errors::kInvalidActionDefaultTitle);
    return nullptr;
  }
  result->set_title(title);

  // Initialize access permissions (optional).
  const base::ListValue* access_list_value = nullptr;
  if (file_browser_handler->HasKey(keys::kFileAccessList)) {
    if (!file_browser_handler->GetList(keys::kFileAccessList,
                                       &access_list_value) ||
        access_list_value->empty()) {
      *error = base::ASCIIToUTF16(errors::kInvalidFileAccessList);
      return nullptr;
    }
    for (size_t i = 0; i < access_list_value->GetSize(); ++i) {
      std::string access;
      if (!access_list_value->GetString(i, &access) ||
          result->AddFileAccessPermission(access)) {
        *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidFileAccessValue, base::NumberToString(i));
        return nullptr;
      }
    }
  }
  if (!result->ValidateFileAccessPermissions()) {
    *error = base::ASCIIToUTF16(errors::kInvalidFileAccessList);
    return nullptr;
  }

  // Initialize file filters (mandatory, unless "create" access is specified,
  // in which case is ignored). The list can be empty.
  if (!result->HasCreateAccessPermission()) {
    const base::ListValue* file_filters = nullptr;
    if (!file_browser_handler->HasKey(keys::kFileFilters) ||
        !file_browser_handler->GetList(keys::kFileFilters, &file_filters)) {
      *error = base::ASCIIToUTF16(errors::kInvalidFileFiltersList);
      return nullptr;
    }
    for (size_t i = 0; i < file_filters->GetSize(); ++i) {
      std::string filter;
      if (!file_filters->GetString(i, &filter)) {
        *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidFileFilterValue, base::NumberToString(i));
        return nullptr;
      }
      filter = base::ToLowerASCII(filter);
      if (!base::StartsWith(filter, std::string(url::kFileSystemScheme) + ':',
                            base::CompareCase::SENSITIVE)) {
        *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidURLPatternError, filter);
        return nullptr;
      }
      // The user inputs filesystem:*; we don't actually implement scheme
      // wildcards in URLPattern, so transform to what will match correctly.
      filter.replace(0, 11, "chrome-extension://*/");
      URLPattern pattern(URLPattern::SCHEME_EXTENSION);
      if (pattern.Parse(filter) != URLPattern::ParseResult::kSuccess) {
        *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidURLPatternError, filter);
        return nullptr;
      }
      std::string path = pattern.path();
      bool allowed = path == "/*" || path == "/*.*" ||
          (path.compare(0, 3, "/*.") == 0 &&
           path.find_first_of('*', 3) == std::string::npos);
      if (!allowed) {
        *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidURLPatternError, filter);
        return nullptr;
      }
      result->AddPattern(pattern);
    }
  }

  std::string default_icon;
  // Read the file browser action |default_icon| (optional).
  if (file_browser_handler->HasKey(keys::kActionDefaultIcon)) {
    if (!file_browser_handler->GetString(keys::kActionDefaultIcon,
                                         &default_icon) ||
        default_icon.empty()) {
      *error = base::ASCIIToUTF16(errors::kInvalidActionDefaultIcon);
      return nullptr;
    }
    result->set_icon_path(default_icon);
  }

  return result;
}

// Loads FileBrowserHandlers from |extension_actions| into a list in |result|.
bool LoadFileBrowserHandlers(
    const std::string& extension_id,
    const base::ListValue* extension_actions,
    FileBrowserHandler::List* result,
    base::string16* error) {
  for (const auto& entry : *extension_actions) {
    const base::DictionaryValue* dict;
    if (!entry.GetAsDictionary(&dict)) {
      *error = base::ASCIIToUTF16(errors::kInvalidFileBrowserHandler);
      return false;
    }
    std::unique_ptr<FileBrowserHandler> action =
        LoadFileBrowserHandler(extension_id, dict, error);
    if (!action)
      return false;  // Failed to parse file browser action definition.
    result->push_back(std::move(action));
  }
  return true;
}

}  // namespace

bool FileBrowserHandlerParser::Parse(extensions::Extension* extension,
                                     base::string16* error) {
  const base::Value* file_browser_handlers_value = nullptr;
  if (!extension->manifest()->Get(keys::kFileBrowserHandlers,
                                  &file_browser_handlers_value)) {
    return true;
  }

  if (!extensions::PermissionsParser::HasAPIPermission(
          extension, extensions::APIPermission::ID::kFileBrowserHandler)) {
    extension->AddInstallWarning(extensions::InstallWarning(
        errors::kInvalidFileBrowserHandlerMissingPermission));
    return true;
  }

  const base::ListValue* file_browser_handlers_list_value = nullptr;
  if (!file_browser_handlers_value->GetAsList(
          &file_browser_handlers_list_value)) {
    *error = base::ASCIIToUTF16(errors::kInvalidFileBrowserHandler);
    return false;
  }

  std::unique_ptr<FileBrowserHandlerInfo> info(new FileBrowserHandlerInfo);
  if (!LoadFileBrowserHandlers(extension->id(),
                               file_browser_handlers_list_value,
                               &info->file_browser_handlers, error)) {
    return false;  // Failed to parse file browser actions definition.
  }

  extension->SetManifestData(keys::kFileBrowserHandlers, std::move(info));
  return true;
}

base::span<const char* const> FileBrowserHandlerParser::Keys() const {
  static constexpr const char* kKeys[] = {keys::kFileBrowserHandlers};
  return kKeys;
}
