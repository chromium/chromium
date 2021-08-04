// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/manifest_update_task.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

namespace {

// Note: Keep in sync with GetDefaultManifestFileHandlers() below.
apps::FileHandlers GetDefaultAppsFileHandlers() {
  apps::FileHandler handler;
  handler.action = GURL("http://foo.com/?plaintext");
  apps::FileHandler::AcceptEntry text_entry;
  text_entry.mime_type = "text/plain";
  text_entry.file_extensions = {".txt", ".md"};
  handler.accept = {text_entry};
  return {handler};
}

// Note: Keep in sync with GetDefaultAppsFileHandlers() above.
std::vector<blink::mojom::ManifestFileHandlerPtr>
GetDefaultManifestFileHandlers() {
  std::vector<blink::mojom::ManifestFileHandlerPtr> handlers;
  auto handler = blink::mojom::ManifestFileHandler::New();
  handler->action = GURL("http://foo.com/?plaintext");
  handler->name = u"Text";
  std::vector<std::u16string> extensions = {u".txt", u".md"};
  handler->accept.emplace(u"text/plain", extensions);
  handlers.push_back(std::move(handler));
  return handlers;
}

}  // anonymous namespace

class ManifestUpdateTaskTest : public testing::Test {
 public:
  ManifestUpdateTaskTest() = default;
  ManifestUpdateTaskTest(const ManifestUpdateTaskTest&) = delete;
  ManifestUpdateTaskTest& operator=(const ManifestUpdateTaskTest&) = delete;
  ~ManifestUpdateTaskTest() override = default;
};

// Below tests primarily test file handler comparison after conversion from
// manifest format. Basic tests like added/removed/unchanged handlers are also
// in functional tests at ManifestUpdateManagerBrowserTestWithFileHandling.
TEST_F(ManifestUpdateTaskTest, TestFileHandlersUnchanged) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  apps::FileHandlers new_handlers = CreateFileHandlersFromManifest(
      GetDefaultManifestFileHandlers(), GURL("http://foo.com"));

  EXPECT_EQ(old_handlers, new_handlers);
}

TEST_F(ManifestUpdateTaskTest, TestSecondFileHandlerAdded) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_handlers =
      GetDefaultManifestFileHandlers();
  auto second_handler = blink::mojom::ManifestFileHandler::New();
  second_handler->action = GURL("http://foo.com/?csv");
  second_handler->name = u"Comma-Separated Value";
  std::vector<std::u16string> extensions = {u".csv"};
  second_handler->accept.emplace(u"text/csv", extensions);
  manifest_handlers.push_back(std::move(second_handler));

  apps::FileHandlers new_handlers =
      CreateFileHandlersFromManifest(manifest_handlers, GURL("http://foo.com"));
  EXPECT_NE(old_handlers, new_handlers);
}

// Ignore name changes, because the registrar doesn't store the name.
TEST_F(ManifestUpdateTaskTest, TestFileHandlerChangedName) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_handlers =
      GetDefaultManifestFileHandlers();
  manifest_handlers[0]->name = u"Comma-Separated Values";

  apps::FileHandlers new_handlers =
      CreateFileHandlersFromManifest(manifest_handlers, GURL("http://foo.com"));
  EXPECT_EQ(old_handlers, new_handlers);
}

TEST_F(ManifestUpdateTaskTest, TestFileHandlerChangedAction) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_handlers =
      GetDefaultManifestFileHandlers();
  manifest_handlers[0]->action = GURL("/?csvtext");

  apps::FileHandlers new_handlers =
      CreateFileHandlersFromManifest(manifest_handlers, GURL("http://foo.com"));
  EXPECT_NE(old_handlers, new_handlers);
}

TEST_F(ManifestUpdateTaskTest, TestFileHandlerExtraAccept) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_handlers =
      GetDefaultManifestFileHandlers();
  std::vector<std::u16string> csv_extensions = {u".csv"};
  manifest_handlers[0]->accept.emplace(u"text/csv", csv_extensions);

  apps::FileHandlers new_handlers =
      CreateFileHandlersFromManifest(manifest_handlers, GURL("http://foo.com"));
  EXPECT_NE(old_handlers, new_handlers);
}

TEST_F(ManifestUpdateTaskTest, TestFileHandlerChangedMimeType) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  old_handlers[0].accept[0].mime_type = "text/csv";
  apps::FileHandlers new_handlers = CreateFileHandlersFromManifest(
      GetDefaultManifestFileHandlers(), GURL("http://foo.com"));

  EXPECT_NE(old_handlers, new_handlers);
}

TEST_F(ManifestUpdateTaskTest, TestFileHandlerChangedExtension) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  old_handlers[0].accept[0].file_extensions.emplace(".csv");
  apps::FileHandlers new_handlers = CreateFileHandlersFromManifest(
      GetDefaultManifestFileHandlers(), GURL("http://foo.com"));

  EXPECT_NE(old_handlers, new_handlers);
}

}  // namespace web_app
