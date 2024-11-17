// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/javascript_browser_test.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ash/js_test_api.h"
#include "components/nacl/common/buildflags.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/browser/web_ui.h"
#include "net/base/filename_util.h"

void JavaScriptBrowserTest::AddLibrary(const base::FilePath& library_path) {
  user_libraries_.push_back(library_path);
}

JavaScriptBrowserTest::JavaScriptBrowserTest() = default;

JavaScriptBrowserTest::~JavaScriptBrowserTest() {
}

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

// TODO(dtseng): Make this return bool (success/failure) and remove ASSERt_TRUE
// calls.
void JavaScriptBrowserTest::BuildJavascriptLibraries(
    std::vector<std::u16string>* libraries) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(libraries != nullptr);
  std::vector<base::FilePath>::iterator user_libraries_iterator;
  for (user_libraries_iterator = user_libraries_.begin();
       user_libraries_iterator != user_libraries_.end();
       ++user_libraries_iterator) {
    std::string library_content;
    base::FilePath library_absolute_path;
    std::vector<base::FilePath::StringType> components =
        user_libraries_iterator->GetComponents();
    if (components[0] == FILE_PATH_LITERAL("ROOT_GEN_DIR")) {
      base::FilePath exe_dir;
      base::PathService::Get(base::DIR_EXE, &exe_dir);
      library_absolute_path = exe_dir.AppendASCII("gen");
      for (size_t i = 1; i < components.size(); i++)
        library_absolute_path = library_absolute_path.Append(components[i]);
      library_absolute_path = library_absolute_path.NormalizePathSeparators();
      ASSERT_TRUE(
          base::ReadFileToString(library_absolute_path, &library_content))
          << user_libraries_iterator->value();
    } else if (user_libraries_iterator->IsAbsolute()) {
      library_absolute_path = *user_libraries_iterator;
      ASSERT_TRUE(
          base::ReadFileToString(library_absolute_path, &library_content))
          << user_libraries_iterator->value();
    } else {
      bool ok = false;
      std::vector<base::FilePath>::iterator library_search_path_iterator;
      for (library_search_path_iterator = library_search_paths_.begin();
           library_search_path_iterator != library_search_paths_.end();
           ++library_search_path_iterator) {
        library_absolute_path = base::MakeAbsoluteFilePath(
            library_search_path_iterator->Append(*user_libraries_iterator));
        ok = base::ReadFileToString(library_absolute_path, &library_content);
        if (ok)
          break;
      }
      ASSERT_TRUE(ok) << "User library not found: "
                      << user_libraries_iterator->value();
    }
    library_content.append(";\n");

    // This magic code puts filenames in stack traces.
    library_content.append("//# sourceURL=");
    library_content.append(
        net::FilePathToFileURL(library_absolute_path).spec());
    library_content.append("\n");
    libraries->push_back(base::UTF8ToUTF16(library_content));
  }
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
