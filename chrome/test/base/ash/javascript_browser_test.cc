// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/javascript_browser_test.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ash/js_test_api.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/browser/web_ui.h"
#include "net/base/filename_util.h"

void JavaScriptBrowserTest::AddLibrary(const base::FilePath& library_path) {
  user_libraries_.push_back(library_path);
}

JavaScriptBrowserTest::JavaScriptBrowserTest() = default;

JavaScriptBrowserTest::~JavaScriptBrowserTest() = default;

void JavaScriptBrowserTest::SetUpOnMainThread() {
  JsTestApiConfig config;
  library_search_paths_.push_back(config.search_path);
  DCHECK(user_libraries_.empty());
  user_libraries_ = config.default_libraries;

  // This generated test directory needs to exist for tests using the js2gtest
  // GN template.
  base::FilePath gen_test_data_directory;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_GEN_TEST_DATA,
                                     &gen_test_data_directory));
  library_search_paths_.push_back(gen_test_data_directory);

  base::FilePath source_root_directory;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT,
                                     &source_root_directory));
  library_search_paths_.push_back(source_root_directory);
}

bool JavaScriptBrowserTest::BuildJavascriptLibraries(
    std::vector<std::u16string>* libraries) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!libraries) {
    LOG(ERROR) << "BuildJavascriptLibraries called with null libraries pointer";
    return false;
  }

  // Path processing logic.
  auto resolve_mapped_or_absolute_path =
      [&](const base::FilePath& path) -> std::optional<base::FilePath> {
    const auto components = path.GetComponents();
    if (components.front() == FILE_PATH_LITERAL("ROOT_GEN_DIR")) {
      base::FilePath exe_dir;
      if (!base::PathService::Get(base::DIR_EXE, &exe_dir)) {
        LOG(ERROR) << "Failed to get base::DIR_EXE for ROOT_GEN_DIR: "
                   << path.value();
        return std::nullopt;
      }
      base::FilePath abs_path = exe_dir.AppendASCII("gen");
      for (size_t i = 1; i < components.size(); ++i) {
        abs_path = abs_path.Append(components[i]);
      }
      return abs_path.NormalizePathSeparators();
    }
    return path.IsAbsolute() ? std::optional<base::FilePath>(path)
                             : std::nullopt;
  };

  for (const auto& library_path : user_libraries_) {
    std::string library_content;
    base::FilePath library_absolute_path;
    bool found = false;
    // Resolve mapped paths or absolute paths.
    if (auto resolved_path = resolve_mapped_or_absolute_path(library_path)) {
      if (base::ReadFileToString(*resolved_path, &library_content)) {
        library_absolute_path = *resolved_path;
        found = true;
      } else {
        LOG(ERROR) << "Failed to read resolved library file: "
                   << resolved_path->value();
      }
    } else {
      // Handling relative paths.
      for (const auto& search_path : library_search_paths_) {
        base::FilePath candidate =
            base::MakeAbsoluteFilePath(search_path.Append(library_path));
        if (base::ReadFileToString(candidate, &library_content)) {
          library_absolute_path = candidate;
          found = true;
          break;
        }
      }
    }

    if (!found) {
      LOG(ERROR) << "Failed to load JS library: " << library_path.value();
      return false;
    }

    // This magic code puts filenames in stack traces.
    std::string source_url =
        "//# sourceURL=" +
        net::FilePathToFileURL(library_absolute_path).spec() + "\n";

    library_content.reserve(library_content.size() + source_url.size() + 3);
    library_content += ";\n";
    library_content += source_url;

    libraries->push_back(base::UTF8ToUTF16(library_content));
  }
  return true;
}

std::u16string JavaScriptBrowserTest::BuildRunTestJSCall(
    bool is_async,
    const std::string& function_name,
    base::Value::List test_func_args) {
  auto arguments = base::Value::List()
                       .Append(is_async)
                       .Append(function_name)
                       .Append(std::move(test_func_args));
  return content::WebUI::GetJavascriptCall(std::string("runTest"), arguments);
}

Profile* JavaScriptBrowserTest::GetProfile() const {
  return browser()->profile();
}
