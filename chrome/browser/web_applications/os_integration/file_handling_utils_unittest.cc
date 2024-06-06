// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

TEST(FileHandlerUtilsTest, GetFileExtensionsFromFileHandler) {
  apps::FileHandler file_handler;
  file_handler.action = GURL("https://app.site/open-foo");
  {
    apps::FileHandler::AcceptEntry accept_entry;
    accept_entry.mime_type = "application/foo";
    accept_entry.file_extensions.insert(".foo");
    file_handler.accept.push_back(accept_entry);
  }
  {
    apps::FileHandler::AcceptEntry accept_entry;
    accept_entry.mime_type = "application/foobar";
    accept_entry.file_extensions.insert(".foobar");
    file_handler.accept.push_back(accept_entry);
  }

  std::set<std::string> file_extensions =
      GetFileExtensionsFromFileHandler(file_handler);

  EXPECT_EQ(2u, file_extensions.size());
  EXPECT_THAT(file_extensions,
              testing::UnorderedElementsAre(".foo", ".foobar"));
}

TEST(FileHandlerUtilsTest, GetFileExtensionsFromFileHandlers) {
  apps::FileHandlers file_handlers;

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-foo");
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foobar";
      accept_entry.file_extensions.insert(".foobar");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-bar");
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/bar";
      accept_entry.file_extensions.insert(".bar");
      accept_entry.file_extensions.insert(".baz");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  std::set<std::string> file_extensions =
      GetFileExtensionsFromFileHandlers(file_handlers);

  EXPECT_EQ(4u, file_extensions.size());
  EXPECT_THAT(file_extensions,
              testing::UnorderedElementsAre(".foo", ".foobar", ".bar", ".baz"));
}

TEST(FileHandlerUtilsTest, GetMimeTypesFromFileHandler) {
  apps::FileHandler file_handler;
  file_handler.action = GURL("https://app.site/open-foo");
  {
    apps::FileHandler::AcceptEntry accept_entry;
    accept_entry.mime_type = "application/foo";
    accept_entry.file_extensions.insert(".foo");
    file_handler.accept.push_back(accept_entry);
  }
  {
    apps::FileHandler::AcceptEntry accept_entry;
    accept_entry.mime_type = "application/foobar";
    accept_entry.file_extensions.insert(".foobar");
    file_handler.accept.push_back(accept_entry);
  }
  std::set<std::string> mime_types = GetMimeTypesFromFileHandler(file_handler);

  EXPECT_EQ(2u, mime_types.size());
  EXPECT_THAT(mime_types, testing::UnorderedElementsAre("application/foo",
                                                        "application/foobar"));
}

TEST(FileHandlerUtilsTest, GetMimeTypesFromFileHandlers) {
  apps::FileHandlers file_handlers;

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-foo");
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foobar";
      accept_entry.file_extensions.insert(".foobar");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-bar");
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/bar";
      accept_entry.file_extensions.insert(".bar");
      accept_entry.file_extensions.insert(".baz");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  std::set<std::string> mime_types =
      GetMimeTypesFromFileHandlers(file_handlers);

  EXPECT_EQ(3u, mime_types.size());
  EXPECT_THAT(mime_types,
              testing::UnorderedElementsAre(
                  "application/foo", "application/foobar", "application/bar"));
}

}  // namespace web_app
