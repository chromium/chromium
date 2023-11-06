// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/chromedriver/chrome/chrome_finder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool PathIn(const std::vector<base::FilePath>& list,
            const base::FilePath& path) {
  return base::Contains(list, path);
}

void AssertFound(const base::FilePath& found,
                 const std::vector<base::FilePath>& existing_paths,
                 const std::vector<base::FilePath>& rel_paths,
                 const std::vector<base::FilePath>& locations) {
  base::FilePath exe;
  ASSERT_TRUE(internal::FindExe(base::BindRepeating(&PathIn, existing_paths),
                                rel_paths, locations, exe));
  ASSERT_EQ(found, exe);
}

}  // namespace

TEST(ChromeFinderTest, FindExeFound) {
  base::FilePath found =
      base::FilePath().AppendASCII("exists").AppendASCII("exists");
  std::vector<base::FilePath> existing_paths;
  existing_paths.push_back(found);
  std::vector<base::FilePath> rel_paths;
  rel_paths.push_back(found.BaseName());
  std::vector<base::FilePath> locations;
  locations.push_back(found.DirName());
  ASSERT_NO_FATAL_FAILURE(
      AssertFound(found, existing_paths, rel_paths, locations));
}

TEST(ChromeFinderTest, FindExeShouldGoInOrder) {
  base::FilePath dir(FILE_PATH_LITERAL("dir"));
  base::FilePath first = dir.AppendASCII("first");
  base::FilePath second = dir.AppendASCII("second");
  std::vector<base::FilePath> existing_paths;
  existing_paths.push_back(first);
  existing_paths.push_back(second);
  std::vector<base::FilePath> rel_paths;
  rel_paths.push_back(first.BaseName());
  rel_paths.push_back(second.BaseName());
  std::vector<base::FilePath> locations;
  locations.push_back(dir);
  ASSERT_NO_FATAL_FAILURE(
      AssertFound(first, existing_paths, rel_paths, locations));
}

TEST(ChromeFinderTest, FindExeShouldPreferExeNameOverDir) {
  base::FilePath dir1(FILE_PATH_LITERAL("dir1"));
  base::FilePath dir2(FILE_PATH_LITERAL("dir2"));
  base::FilePath preferred(FILE_PATH_LITERAL("preferred"));
  base::FilePath nonpreferred(FILE_PATH_LITERAL("nonpreferred"));
  std::vector<base::FilePath> existing_paths;
  existing_paths.push_back(dir2.Append(preferred));
  existing_paths.push_back(dir1.Append(nonpreferred));
  std::vector<base::FilePath> rel_paths;
  rel_paths.push_back(preferred);
  rel_paths.push_back(nonpreferred);
  std::vector<base::FilePath> locations;
  locations.push_back(dir1);
  locations.push_back(dir2);
  ASSERT_NO_FATAL_FAILURE(AssertFound(
      dir2.Append(preferred), existing_paths, rel_paths, locations));
}

TEST(ChromeFinderTest, FindExeNotFound) {
  base::FilePath found =
      base::FilePath().AppendASCII("exists").AppendASCII("exists");
  std::vector<base::FilePath> existing_paths;
  std::vector<base::FilePath> rel_paths;
  rel_paths.push_back(found.BaseName());
  std::vector<base::FilePath> locations;
  locations.push_back(found.DirName());
  base::FilePath exe;
  ASSERT_FALSE(internal::FindExe(base::BindRepeating(&PathIn, existing_paths),
                                 rel_paths, locations, exe));
}

TEST(ChromeFinderTest, NoCrash) {
  // It's not worthwhile to check the validity of the path, so just check
  // for crashes.
  base::FilePath exe;
  FindBrowser("chrome", exe);
  FindBrowser("chrome-headless-shell", exe);
  FindBrowser("", exe);
  FindBrowser("quick-brown-fox", exe);
}

TEST(ChromeFinderTest, FindBrowserSearchesForChrome) {
  // Verify that FindBrowser searches for Chrome when requested to
  // find Chrome.
  base::FilePath exe;
  auto exists_func = base::BindRepeating([](const base::FilePath& path) {
    std::vector<base::FilePath::StringType> components = path.GetComponents();
    EXPECT_FALSE(components.empty());
    if (components.empty()) {
      return false;
    }
    return components.back() == chrome::kBrowserProcessExecutableName;
  });
  EXPECT_TRUE(FindBrowser("chrome", exists_func, exe));
  // Empty string defaults to "chrome". This mimics the situation when the
  // "browserName" capability is not provided.
  EXPECT_TRUE(FindBrowser("", exists_func, exe));
}

TEST(ChromeFinderTest, FindBrowserSearchesForHeadlessShell) {
  // Verify that FindBrowser searches for HeadlessShell when requested to
  // find HeadlessShell.
  base::FilePath exe;
  auto exists_func = base::BindRepeating([](const base::FilePath& path) {
    std::vector<base::FilePath::StringType> components = path.GetComponents();
    EXPECT_FALSE(components.empty());
    if (components.empty()) {
      return false;
    }
    return base::StartsWith(components.back(),
                            FILE_PATH_LITERAL("chrome-headless-shell"));
  });
  EXPECT_TRUE(FindBrowser("chrome-headless-shell", exists_func, exe));
}

TEST(ChromeFinderTest, FindBrowserDoesNotSearchForUnsupportedBrowser) {
  // Verify that FindBrowser returns false for unknown browsers.
  base::FilePath exe;
  auto exists_func =
      base::BindRepeating([](const base::FilePath& path) { return true; });
  EXPECT_FALSE(FindBrowser("quick-brown-fox", exists_func, exe));
}

TEST(ChromeFinderTest, FindBrowserDoesNotSearchForChrome) {
  // Verify that FindBrowser does not search for Chrome when requested to
  // find HeadlessShell.
  base::FilePath exe;
  auto exists_func = base::BindRepeating([](const base::FilePath& path) {
    std::vector<base::FilePath::StringType> components = path.GetComponents();
    EXPECT_FALSE(components.empty());
    if (components.empty()) {
      return false;
    }
    return components.back() == chrome::kBrowserProcessExecutableName;
  });
  EXPECT_FALSE(FindBrowser("chrome-headless-shell", exists_func, exe));
}

TEST(ChromeFinderTest, FindBrowserDoesNotSearchForHeadlessShell) {
  // Verify that FindBrowser does not search for HeadlessShell when requested to
  // find Chrome.
  base::FilePath exe;
  auto exists_func = base::BindRepeating([](const base::FilePath& path) {
    std::vector<base::FilePath::StringType> components = path.GetComponents();
    EXPECT_FALSE(components.empty());
    if (components.empty()) {
      return false;
    }
    return base::StartsWith(components.back(),
                            FILE_PATH_LITERAL("chrome-headless-shell"));
  });
  EXPECT_FALSE(FindBrowser("chrome", exists_func, exe));
  // Empty string defaults to "chrome". This mimics the situation when the
  // "browserName" capability is not provided.
  EXPECT_FALSE(FindBrowser("", exists_func, exe));
}
