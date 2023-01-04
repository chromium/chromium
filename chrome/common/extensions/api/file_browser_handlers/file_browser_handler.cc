// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/check.h"
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

FileBrowserHandlerInfo::FileBrowserHandlerInfo() = default;

FileBrowserHandlerInfo::~FileBrowserHandlerInfo() = default;

}  // namespace

FileBrowserHandler::FileBrowserHandler()
    : file_access_permission_flags_(kPermissionsNotDefined) {
}

FileBrowserHandler::~FileBrowserHandler() = default;

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
  if (!info) {
    return nullptr;
  }

  return &info->file_browser_handlers;
}

// static
const FileBrowserHandler* FileBrowserHandler::FindForActionId(
    const extensions::Extension* extension,
    const std::string& action_id) {
  for (const auto& handler : *FileBrowserHandler::GetHandlers(extension)) {
    if (handler->id() == action_id)
      return handler.get();
  }
  return nullptr;
}

FileBrowserHandlerParser::FileBrowserHandlerParser() = default;

FileBrowserHandlerParser::~FileBrowserHandlerParser() = default;

namespace {

std::unique_ptr<FileBrowserHandler> LoadFileBrowserHandler(
    const std::string& extension_id,
    const base::Value::Dict* file_browser_handler,
    std::u16string* error) {
  std::unique_ptr<FileBrowserHandler> result(new FileBrowserHandler());
  result->set_extension_id(extension_id);

  const std::string* handler_id =
      file_browser_handler->FindString(keys::kFileBrowserHandlerId);
  // Read the file action |id| (mandatory).
  if (!handler_id) {
    *error = errors::kInvalidFileBrowserHandlerId;
    return nullptr;
  }
  result->set_id(*handler_id);

  // Read the page action title from |default_title| (mandatory).
  const std::string* title =
      file_browser_handler->FindString(keys::kActionDefaultTitle);
  if (!title) {
    *error = errors::kInvalidActionDefaultTitle;
    return nullptr;
  }
  result->set_title(*title);

  // Initialize access permissions (optional).
  const base::Value* access_list_value =
      file_browser_handler->Find(keys::kFileAccessList);
  if (access_list_value) {
    if (!access_list_value->is_list() || access_list_value->GetList().empty()) {
      *error = errors::kInvalidFileAccessList;
      return nullptr;
    }
    const base::Value::List& access_list_view = access_list_value->GetList();
    for (size_t i = 0; i < access_list_view.size(); ++i) {
      const std::string* access = access_list_view[i].GetIfString();
      if (!access || result->AddFileAccessPermission(*access)) {
        *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidFileAccessValue, base::NumberToString(i));
        return nullptr;
      }
    }
  }
  if (!result->ValidateFileAccessPermissions()) {
    *error = errors::kInvalidFileAccessList;
    return nullptr;
  }

  // Initialize file filters (mandatory, unless "create" access is specified,
  // in which case is ignored). The list can be empty.
  if (!result->HasCreateAccessPermission()) {
    const base::Value::List* file_filters_list =
        file_browser_handler->FindList(keys::kFileFilters);
    if (!file_filters_list) {
      *error = errors::kInvalidFileFiltersList;
      return nullptr;
    }
    for (size_t i = 0; i < file_filters_list->size(); ++i) {
      const std::string* filter_in = (*file_filters_list)[i].GetIfString();
      if (!filter_in) {
        *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidFileFilterValue, base::NumberToString(i));
        return nullptr;
      }
      std::string filter = base::ToLowerASCII(*filter_in);
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

  // Read the file browser action |default_icon| (optional).
  if (const base::Value* default_icon_val =
          file_browser_handler->Find(keys::kActionDefaultIcon)) {
    const std::string* default_icon = default_icon_val->GetIfString();
    if (!default_icon || default_icon->empty()) {
      *error = errors::kInvalidActionDefaultIcon;
      return nullptr;
    }
    result->set_icon_path(*default_icon);
  }

  return result;
}

// Loads FileBrowserHandlers from |extension_actions| into a list in |result|.
bool LoadFileBrowserHandlers(const std::string& extension_id,
                             const base::Value::List& extension_actions,
                             FileBrowserHandler::List* result,
                             std::u16string* error) {
  for (const auto& entry : extension_actions) {
    const base::Value::Dict* dict = entry.GetIfDict();
    if (!dict) {
      *error = errors::kInvalidFileBrowserHandler16;
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
                                     std::u16string* error) {
  const base::Value* file_browser_handlers_value =
      extension->manifest()->FindPath(keys::kFileBrowserHandlers);
  if (file_browser_handlers_value == nullptr) {
    return true;
  }

  if (!extensions::PermissionsParser::HasAPIPermission(
          extension, extensions::mojom::APIPermissionID::kFileBrowserHandler)) {
    extension->AddInstallWarning(extensions::InstallWarning(
        errors::kInvalidFileBrowserHandlerMissingPermission));
    return true;
  }

  if (!file_browser_handlers_value->is_list()) {
    *error = errors::kInvalidFileBrowserHandler16;
    return false;
  }

  std::unique_ptr<FileBrowserHandlerInfo> info(new FileBrowserHandlerInfo);
  if (!LoadFileBrowserHandlers(extension->id(),
                               file_browser_handlers_value->GetList(),
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
