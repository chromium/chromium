// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_FILE_BROWSER_HANDLERS_FILE_BROWSER_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_FILE_BROWSER_HANDLERS_FILE_BROWSER_HANDLER_H_

#include <string>
#include <vector>

#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"

class GURL;
class URLPattern;

// FileBrowserHandler encapsulates the state of a file browser action.
class FileBrowserHandler {
 public:
  using List = std::vector<std::unique_ptr<FileBrowserHandler>>;

  FileBrowserHandler();

  FileBrowserHandler(const FileBrowserHandler&) = delete;
  FileBrowserHandler& operator=(const FileBrowserHandler&) = delete;

  ~FileBrowserHandler();

  // extension id
  extensions::ExtensionId extension_id() const { return extension_id_; }
  void set_extension_id(const std::string& extension_id) {
    extension_id_ = extension_id;
  }

  // action id
  const std::string& id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

  // default title
  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }

  // File schema URL patterns.
  const extensions::URLPatternSet& file_url_patterns() const {
    return url_set_;
  }
  void AddPattern(const URLPattern& pattern);
  bool MatchesURL(const GURL& url) const;
  void ClearPatterns();

  // Action icon path.
  const std::string& icon_path() const { return default_icon_path_; }
  void set_icon_path(const std::string& path) {
    default_icon_path_ = path;
  }

  // File access permissions.
  // Adjusts file_access_permission_flags_ to allow specified permission.
  bool AddFileAccessPermission(const std::string& permission_str);
  // Checks that specified file access permissions are valid (all set
  // permissions are valid and there is no other permission specified with
  // "create")
  // If no access permissions were set, initialize them to default value.
  bool ValidateFileAccessPermissions();
  // Checks if handler has read access.
  bool CanRead() const;
  // Checks if handler has write access.
  bool CanWrite() const;
  // Checks if handler has "create" access specified.
  bool HasCreateAccessPermission() const;

  // Returns the file browser handlers associated with the |extension|.
  static List* GetHandlers(const extensions::Extension* extension);

  // Returns file browser handler in |extension| matching |action_id|, or
  // nullptr if not found.
  static const FileBrowserHandler* FindForActionId(
      const extensions::Extension* extension,
      const std::string& action_id);

 private:
  // The id for the extension this action belongs to (as defined in the
  // extension manifest).
  extensions::ExtensionId extension_id_;
  std::string title_;
  std::string default_icon_path_;
  // The id for the FileBrowserHandler, for example: "PdfFileAction".
  std::string id_;
  unsigned int file_access_permission_flags_;

  // A list of file filters.
  extensions::URLPatternSet url_set_;
};

// Parses the "file_browser_handlers" extension manifest key.
class FileBrowserHandlerParser : public extensions::ManifestHandler {
 public:
  FileBrowserHandlerParser();

  FileBrowserHandlerParser(const FileBrowserHandlerParser&) = delete;
  FileBrowserHandlerParser& operator=(const FileBrowserHandlerParser&) = delete;

  ~FileBrowserHandlerParser() override;

  bool Parse(extensions::Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

#endif  // CHROME_COMMON_EXTENSIONS_API_FILE_BROWSER_HANDLERS_FILE_BROWSER_HANDLER_H_
