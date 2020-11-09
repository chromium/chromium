// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_file_handler_registration.h"

#include <map>
#include <string>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

typedef WebAppTest WebAppFileHandlerRegistrationLinuxTest;

namespace {

using AcceptMap = std::map<std::string, base::flat_set<std::string>>;

apps::FileHandler GetTestFileHandler(const std::string& action,
                                     const AcceptMap& accept_map) {
  apps::FileHandler file_handler;
  file_handler.action = GURL(action);
  for (const auto& elem : accept_map) {
    apps::FileHandler::AcceptEntry accept_entry;
    accept_entry.mime_type = elem.first;
    accept_entry.file_extensions.insert(elem.second.begin(), elem.second.end());
    file_handler.accept.push_back(accept_entry);
  }
  return file_handler;
}

}  // namespace

TEST_F(WebAppFileHandlerRegistrationLinuxTest,
       RegisterMimeTypesLocalVariablesAreCorrect) {
  Profile* test_profile = profile();
  const AppId& app_id("app-id");

  apps::FileHandlers file_handlers;
  file_handlers.push_back(GetTestFileHandler(
      "https://site.api/open-foo",
      {{"application/foo", {".foo"}}, {"application/foobar", {".foobar"}}}));
  file_handlers.push_back(GetTestFileHandler(
      "https://site.api/open-bar", {{"application/bar", {".bar", ".baz"}}}));

  base::FilePath expected_filename =
      shell_integration_linux::GetMimeTypesRegistrationFilename(
          test_profile->GetPath(), app_id);
  std::string expected_file_contents =
      shell_integration_linux::GetMimeTypesRegistrationFileContents(
          file_handlers);

  RegisterMimeTypesOnLinuxCallback callback = base::BindLambdaForTesting(
      [expected_filename, expected_file_contents](base::FilePath filename,
                                                  std::string file_contents) {
        EXPECT_EQ(filename, expected_filename);
        EXPECT_EQ(file_contents, expected_file_contents);
        return true;
      });

  RegisterMimeTypesOnLinux(app_id, test_profile, file_handlers,
                           std::move(callback));
}

}  // namespace web_app
