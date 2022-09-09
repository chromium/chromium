// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/javascript_browser_test.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/js_test_api.h"
#include "components/nacl/common/buildflags.h"
#include "content/public/browser/web_ui.h"
#include "net/base/filename_util.h"

void JavaScriptBrowserTest::AddLibrary(const base::FilePath& library_path) {
  user_libraries_.push_back(library_path);
}

JavaScriptBrowserTest::JavaScriptBrowserTest() {
#if BUILDFLAG(IS_CHROMEOS_ASH)

#endif
}

JavaScriptBrowserTest::~JavaScriptBrowserTest() {
}

void JavaScriptBrowserTest::SetUpInProcessBrowserTestFixture() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash_starter_ = std::make_unique<test::AshBrowserTestStarter>();
  if (ash_starter_->HasLacrosArgument())
    ASSERT_TRUE(ash_starter_->PrepareEnvironmentForLacros());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void JavaScriptBrowserTest::TearDownInProcessBrowserTestFixture() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash_starter_->HasLacrosArgument())
    ash_starter_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void JavaScriptBrowserTest::SetUpOnMainThread() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash_starter_->HasLacrosArgument())
    ash_starter_->StartLacros(this);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  JsTestApiConfig config;
  library_search_paths_.push_back(config.search_path);
  DCHECK(user_libraries_.empty());
  user_libraries_ = config.default_libraries;

// When the sanitizers (ASAN/MSAN/TSAN) are enabled, the WebUI tests
// which use this generated directory are disabled in the build.
// However, the generated directory is there if NaCl is enabled --
// though it's usually disabled on the bots when the sanitizers are
// enabled. Also, it seems some ChromeOS-specific tests use the
// js2gtest GN template.
#if (!defined(MEMORY_SANITIZER) && !defined(ADDRESS_SANITIZER) && \
     !defined(LEAK_SANITIZER)) ||                                 \
    BUILDFLAG(ENABLE_NACL) || BUILDFLAG(IS_CHROMEOS_ASH)
  base::FilePath gen_test_data_directory;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_GEN_TEST_DATA,
                                     &gen_test_data_directory));
  library_search_paths_.push_back(gen_test_data_directory);
#endif

  base::FilePath source_root_directory;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_directory));
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
    std::vector<base::Value> test_func_args) {
  std::vector<base::Value> arguments;
  arguments.emplace_back(is_async);
  arguments.emplace_back(function_name);
  base::Value::List baked_argument_list;
  for (auto& arg : test_func_args)
    baked_argument_list.Append(std::move(arg));
  arguments.emplace_back(std::move(baked_argument_list));

  std::vector<base::ValueView> view_vector;
  view_vector.reserve(arguments.size());
  for (const auto& argument : arguments)
    view_vector.push_back(argument);
  return content::WebUI::GetJavascriptCall(std::string("runTest"), view_vector);
}
