// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/manifest_update_task.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

namespace {

// Note: Keep in sync with GetDefaultManifestFileHandlers() below.
apps::FileHandlers GetDefaultAppsFileHandlers() {
  apps::FileHandler handler;
  handler.action = GURL("http://foo.com/?plaintext");
  handler.display_name = u"Text";
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

TEST_F(ManifestUpdateTaskTest, TestFileHandlerChangedName) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_handlers =
      GetDefaultManifestFileHandlers();
  manifest_handlers[0]->name = u"Comma-Separated Values";

  apps::FileHandlers new_handlers =
      CreateFileHandlersFromManifest(manifest_handlers, GURL("http://foo.com"));
  EXPECT_NE(old_handlers, new_handlers);
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

std::vector<apps::IconInfo> GenerateIconInfosFrom(
    const IconBitmaps& downloaded) {
  std::vector<apps::IconInfo> result;
  for (auto entry : downloaded.any) {
    apps::IconInfo icon_info(GURL(), entry.first);
    icon_info.purpose = apps::IconInfo::Purpose::kAny;
    result.push_back(icon_info);
  }
  for (auto entry : downloaded.maskable) {
    apps::IconInfo icon_info(GURL(), entry.first);
    icon_info.purpose = apps::IconInfo::Purpose::kMaskable;
    result.push_back(icon_info);
  }
  for (auto entry : downloaded.monochrome) {
    apps::IconInfo icon_info(GURL(), entry.first);
    icon_info.purpose = apps::IconInfo::Purpose::kMonochrome;
    result.push_back(icon_info);
  }
  return result;
}

TEST_F(ManifestUpdateTaskTest, TestImageComparison) {
  {
    // Test case: Find first difference with two empty IconBitmaps as input
    // should report no differences.
    IconBitmaps on_disk;
    IconBitmaps downloaded;
    IconDiff diff = HaveIconBitmapsChanged(
        on_disk, downloaded, GenerateIconInfosFrom(on_disk),
        GenerateIconInfosFrom(downloaded),
        /* end_when_mismatch_detected= */ true);
    EXPECT_EQ(NO_CHANGE_DETECTED, diff.diff_results);
    EXPECT_TRUE(diff.before.drawsNothing());
    EXPECT_TRUE(diff.after.drawsNothing());
  }
  {
    // Test case: Find all differences with two empty IconBitmaps as input
    // should report no differences.
    IconBitmaps on_disk;
    IconBitmaps downloaded;

    IconDiff diff = HaveIconBitmapsChanged(
        on_disk, downloaded, GenerateIconInfosFrom(on_disk),
        GenerateIconInfosFrom(downloaded),
        /* end_when_mismatch_detected= */ false);
    EXPECT_EQ(NO_CHANGE_DETECTED, diff.diff_results);
    EXPECT_TRUE(diff.before.drawsNothing());
    EXPECT_TRUE(diff.after.drawsNothing());
  }

  {
    // Test case: Find first difference when one new image has been downloaded
    // should report size mismatch.
    IconBitmaps on_disk;
    IconBitmaps downloaded;
    AddGeneratedIcon(&downloaded.any, icon_size::k512, SK_ColorYELLOW);

    IconDiff diff = HaveIconBitmapsChanged(
        on_disk, downloaded, GenerateIconInfosFrom(on_disk),
        GenerateIconInfosFrom(downloaded),
        /* end_when_mismatch_detected= */ true);
    EXPECT_EQ(MISMATCHED_IMAGE_SIZES, diff.diff_results);
    EXPECT_TRUE(diff.before.drawsNothing());
    EXPECT_TRUE(diff.after.drawsNothing());
  }
  {
    // Test case: Find all differences when one new image has been downloaded
    // should report size mismatch.
    IconBitmaps on_disk;
    IconBitmaps downloaded;
    AddGeneratedIcon(&downloaded.any, icon_size::k512, SK_ColorYELLOW);

    IconDiff diff = HaveIconBitmapsChanged(
        on_disk, downloaded, GenerateIconInfosFrom(on_disk),
        GenerateIconInfosFrom(downloaded),
        /* end_when_mismatch_detected= */ false);
    EXPECT_EQ(MISMATCHED_IMAGE_SIZES, diff.diff_results);
    EXPECT_TRUE(diff.before.drawsNothing());
    EXPECT_TRUE(diff.after.drawsNothing());
  }

  {
    // Test case: Find first difference when one image has been removed
    // should report size mismatch.
    IconBitmaps on_disk;
    IconBitmaps downloaded;
    AddGeneratedIcon(&on_disk.any, icon_size::k512, SK_ColorYELLOW);

    IconDiff diff = HaveIconBitmapsChanged(
        on_disk, downloaded, GenerateIconInfosFrom(on_disk),
        GenerateIconInfosFrom(downloaded),
        /* end_when_mismatch_detected= */ true);
    EXPECT_EQ(MISMATCHED_IMAGE_SIZES, diff.diff_results);
    EXPECT_TRUE(diff.before.drawsNothing());
    EXPECT_TRUE(diff.after.drawsNothing());
  }
  {
    // Test case: Find all differences when one new image has been removed
    // should report size mismatch.
    IconBitmaps on_disk;
    IconBitmaps downloaded;
    AddGeneratedIcon(&on_disk.any, icon_size::k512, SK_ColorYELLOW);

    IconDiff diff = HaveIconBitmapsChanged(
        on_disk, downloaded, GenerateIconInfosFrom(on_disk),
        GenerateIconInfosFrom(downloaded),
        /* end_when_mismatch_detected= */ false);
    EXPECT_EQ(MISMATCHED_IMAGE_SIZES, diff.diff_results);
    EXPECT_TRUE(diff.before.drawsNothing());
    EXPECT_TRUE(diff.after.drawsNothing());
  }

  {
    // Test case: Find first difference, when one image has been removed and one
    // added, should report size mismatch.
    IconBitmaps on_disk;
    IconBitmaps downloaded;
    AddGeneratedIcon(&on_disk.any, icon_size::k256, SK_ColorRED);
    AddGeneratedIcon(&downloaded.any, icon_size::k512, SK_ColorYELLOW);

    IconDiff diff = HaveIconBitmapsChanged(on_disk, downloaded,
                                           GenerateIconInfosFrom(on_disk),
                                           GenerateIconInfosFrom(downloaded),
                                           /* end_when_mismatch_detected= */
                                           true);
    // First mismatch found will be the added image, then it will stop.
    EXPECT_EQ(MISMATCHED_IMAGE_SIZES, diff.diff_results);
    EXPECT_TRUE(diff.before.drawsNothing());
    EXPECT_TRUE(diff.after.drawsNothing());
  }
  {
    // Test case: Find all differences, when one image has been removed and one
    // added, should report size mismatch.
    IconBitmaps on_disk;
    IconBitmaps downloaded;
    AddGeneratedIcon(&on_disk.any, icon_size::k256, SK_ColorRED);
    AddGeneratedIcon(&downloaded.any, icon_size::k512, SK_ColorYELLOW);

    IconDiff diff = HaveIconBitmapsChanged(on_disk, downloaded,
                                           GenerateIconInfosFrom(on_disk),
                                           GenerateIconInfosFrom(downloaded),
                                           /* end_when_mismatch_detected=
                                            */
                                           false);
    EXPECT_EQ(MISMATCHED_IMAGE_SIZES, diff.diff_results);
    EXPECT_TRUE(diff.before.drawsNothing());
    EXPECT_TRUE(diff.after.drawsNothing());
  }

  {
    // Test case: Find first difference, when one image has been removed and one
    // added (but across maps), should report size mismatch.
    IconBitmaps on_disk;
    IconBitmaps downloaded;
    AddGeneratedIcon(&on_disk.maskable, icon_size::k256, SK_ColorRED);
    AddGeneratedIcon(&downloaded.monochrome, icon_size::k512, SK_ColorYELLOW);

    IconDiff diff = HaveIconBitmapsChanged(on_disk, downloaded,
                                           GenerateIconInfosFrom(on_disk),
                                           GenerateIconInfosFrom(downloaded),
                                           /* end_when_mismatch_detected=
                                            */
                                           true);
    // First mismatch found will be the fact that one of the maps has changed
    // size.
    EXPECT_EQ(MISMATCHED_IMAGE_SIZES, diff.diff_results);
    EXPECT_TRUE(diff.before.drawsNothing());
    EXPECT_TRUE(diff.after.drawsNothing());
  }
  {
    // Test case: Find all differences, when one image has been removed and one
    // added (but across maps), should report size mismatch.
    IconBitmaps on_disk;
    IconBitmaps downloaded;
    AddGeneratedIcon(&on_disk.maskable, icon_size::k256, SK_ColorRED);
    AddGeneratedIcon(&downloaded.monochrome, icon_size::k512, SK_ColorYELLOW);

    IconDiff diff = HaveIconBitmapsChanged(on_disk, downloaded,
                                           GenerateIconInfosFrom(on_disk),
                                           GenerateIconInfosFrom(downloaded),
                                           /* end_when_mismatch_detected= */
                                           false);
    EXPECT_EQ(MISMATCHED_IMAGE_SIZES, diff.diff_results);
    EXPECT_TRUE(diff.before.drawsNothing());
    EXPECT_TRUE(diff.after.drawsNothing());
  }

  {
    // Test case: Find first difference, when one image has had its bits
    // updated, should return ONE_OR_MORE_ICONS_CHANGED.
    IconBitmaps on_disk;
    IconBitmaps downloaded;
    AddGeneratedIcon(&on_disk.any, icon_size::k256, SK_ColorRED);
    AddGeneratedIcon(&downloaded.any, icon_size::k256, SK_ColorYELLOW);

    IconDiff diff = HaveIconBitmapsChanged(on_disk, downloaded,
                                           GenerateIconInfosFrom(on_disk),
                                           GenerateIconInfosFrom(downloaded),
                                           /* end_when_mismatch_detected= */
                                           true);
    EXPECT_EQ(ONE_OR_MORE_ICONS_CHANGED, diff.diff_results);
    // The expectation here might, at a glance, seem unusual because there *has*
    // been a change in only a single icon. However, this was detected via the
    // short pass, which does not provide |before| and |after| images (only the
    // longer pass will know whether more images changed).
    EXPECT_TRUE(diff.before.drawsNothing());
    EXPECT_TRUE(diff.after.drawsNothing());
  }
  {
    // Test case: Find all differences, when one image has had its bits
    // updated, should return SINGLE_ICON_CHANGED.
    IconBitmaps on_disk;
    IconBitmaps downloaded;
    AddGeneratedIcon(&on_disk.any, icon_size::k256, SK_ColorRED);
    AddGeneratedIcon(&downloaded.any, icon_size::k256, SK_ColorYELLOW);

    IconDiff diff = HaveIconBitmapsChanged(
        on_disk, downloaded, GenerateIconInfosFrom(on_disk),
        GenerateIconInfosFrom(downloaded),
        /* end_when_mismatch_detected= */ false);
    EXPECT_EQ(SINGLE_ICON_CHANGED, diff.diff_results);
    // The function has checked all possibilities and is able to provide before
    // and after images, because it knows only a single image changed.
    EXPECT_FALSE(diff.before.drawsNothing());
    EXPECT_FALSE(diff.after.drawsNothing());
  }

  {
    // Test case: Find first difference, when two images have had their bits
    // updated, should return ONE_OR_MORE_ICONS_CHANGED.
    IconBitmaps on_disk;
    IconBitmaps downloaded;
    AddGeneratedIcon(&on_disk.any, icon_size::k256, SK_ColorRED);
    AddGeneratedIcon(&on_disk.any, icon_size::k512, SK_ColorRED);
    AddGeneratedIcon(&downloaded.any, icon_size::k256, SK_ColorYELLOW);
    AddGeneratedIcon(&downloaded.any, icon_size::k512, SK_ColorYELLOW);

    IconDiff diff = HaveIconBitmapsChanged(on_disk, downloaded,
                                           GenerateIconInfosFrom(on_disk),
                                           GenerateIconInfosFrom(downloaded),
                                           /* end_when_mismatch_detected= */
                                           true);
    EXPECT_EQ(ONE_OR_MORE_ICONS_CHANGED, diff.diff_results);
    // Since more than two images changed, the |before| and |after| isn't
    // provided.
    EXPECT_TRUE(diff.before.drawsNothing());
    EXPECT_TRUE(diff.after.drawsNothing());
  }
  {
    // Test case: Find all differences, when two images have had their bits
    // updated, should return MULTIPLE_ICONS_CHANGED.
    IconBitmaps on_disk;
    IconBitmaps downloaded;
    AddGeneratedIcon(&on_disk.any, icon_size::k256, SK_ColorRED);
    AddGeneratedIcon(&on_disk.any, icon_size::k512, SK_ColorRED);
    AddGeneratedIcon(&downloaded.any, icon_size::k256, SK_ColorYELLOW);
    AddGeneratedIcon(&downloaded.any, icon_size::k512, SK_ColorYELLOW);

    IconDiff diff = HaveIconBitmapsChanged(on_disk, downloaded,
                                           GenerateIconInfosFrom(on_disk),
                                           GenerateIconInfosFrom(downloaded),
                                           /* end_when_mismatch_detected= */
                                           false);
    EXPECT_EQ(MULTIPLE_ICONS_CHANGED, diff.diff_results);
    EXPECT_TRUE(diff.before.drawsNothing());
    EXPECT_TRUE(diff.after.drawsNothing());
  }

  {
    // Test case: Find first difference, when two images have had their bits
    // updated (across |any| and |maskable|), should return
    // ONE_OR_MORE_ICONS_CHANGED.
    IconBitmaps on_disk;
    IconBitmaps downloaded;
    AddGeneratedIcon(&on_disk.any, icon_size::k256, SK_ColorRED);
    AddGeneratedIcon(&on_disk.maskable, icon_size::k512, SK_ColorRED);
    AddGeneratedIcon(&downloaded.any, icon_size::k256, SK_ColorYELLOW);
    AddGeneratedIcon(&downloaded.maskable, icon_size::k512, SK_ColorYELLOW);

    IconDiff diff = HaveIconBitmapsChanged(on_disk, downloaded,
                                           GenerateIconInfosFrom(on_disk),
                                           GenerateIconInfosFrom(downloaded),
                                           /* end_when_mismatch_detected= */
                                           true);
    EXPECT_EQ(ONE_OR_MORE_ICONS_CHANGED, diff.diff_results);
    // Since more than two images changed, the |before| and |after| isn't
    // provided.
    EXPECT_TRUE(diff.before.drawsNothing());
    EXPECT_TRUE(diff.after.drawsNothing());
  }
  {
    // Test case: Find all differences, when two images have had their bits
    // updated (across |any| and |maskable|), should return
    // MULTIPLE_ICONS_CHANGED.
    IconBitmaps on_disk;
    IconBitmaps downloaded;
    AddGeneratedIcon(&on_disk.any, icon_size::k256, SK_ColorRED);
    AddGeneratedIcon(&on_disk.maskable, icon_size::k512, SK_ColorRED);
    AddGeneratedIcon(&downloaded.any, icon_size::k256, SK_ColorYELLOW);
    AddGeneratedIcon(&downloaded.maskable, icon_size::k512, SK_ColorYELLOW);

    IconDiff diff = HaveIconBitmapsChanged(on_disk, downloaded,
                                           GenerateIconInfosFrom(on_disk),
                                           GenerateIconInfosFrom(downloaded),
                                           /* end_when_mismatch_detected= */
                                           false);
    EXPECT_EQ(MULTIPLE_ICONS_CHANGED, diff.diff_results);
    EXPECT_TRUE(diff.before.drawsNothing());
    EXPECT_TRUE(diff.after.drawsNothing());
  }

  {
    // Test case: Find first difference, when two images have had their bits
    // updated (across |maskable| and |monochrome|), should return
    // ONE_OR_MORE_ICON_CHANGED.
    IconBitmaps on_disk;
    IconBitmaps downloaded;
    AddGeneratedIcon(&on_disk.maskable, icon_size::k256, SK_ColorRED);
    AddGeneratedIcon(&on_disk.monochrome, icon_size::k512, SK_ColorRED);
    AddGeneratedIcon(&downloaded.maskable, icon_size::k256, SK_ColorYELLOW);
    AddGeneratedIcon(&downloaded.monochrome, icon_size::k512, SK_ColorYELLOW);

    IconDiff diff = HaveIconBitmapsChanged(on_disk, downloaded,
                                           GenerateIconInfosFrom(on_disk),
                                           GenerateIconInfosFrom(downloaded),
                                           /* end_when_mismatch_detected= */
                                           true);
    EXPECT_EQ(ONE_OR_MORE_ICONS_CHANGED, diff.diff_results);
    // Since more than two images changed, the |before| and |after| isn't
    // provided.
    EXPECT_TRUE(diff.before.drawsNothing());
    EXPECT_TRUE(diff.after.drawsNothing());
  }
  {
    // Test case: Find all differences, when two images have had their bits
    // updated (across |maskable| and |monochrome|), should return
    // MULTIPLE_ICONS_CHANGED.
    IconBitmaps on_disk;
    IconBitmaps downloaded;
    AddGeneratedIcon(&on_disk.maskable, icon_size::k256, SK_ColorRED);
    AddGeneratedIcon(&on_disk.monochrome, icon_size::k512, SK_ColorRED);
    AddGeneratedIcon(&downloaded.maskable, icon_size::k256, SK_ColorYELLOW);
    AddGeneratedIcon(&downloaded.monochrome, icon_size::k512, SK_ColorYELLOW);

    IconDiff diff = HaveIconBitmapsChanged(on_disk, downloaded,
                                           GenerateIconInfosFrom(on_disk),
                                           GenerateIconInfosFrom(downloaded),
                                           /* end_when_mismatch_detected= */
                                           false);
    EXPECT_EQ(MULTIPLE_ICONS_CHANGED, diff.diff_results);
    EXPECT_TRUE(diff.before.drawsNothing());
    EXPECT_TRUE(diff.after.drawsNothing());
  }
}

}  // namespace web_app
