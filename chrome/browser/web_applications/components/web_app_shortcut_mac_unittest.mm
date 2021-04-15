// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/web_applications/components/web_app_shortcut_mac.h"

#import <Cocoa/Cocoa.h>
#include <errno.h>
#include <stddef.h>
#include <sys/xattr.h>

#include <memory>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#import "chrome/common/mac/app_mode_common.h"
#include "chrome/grit/theme_resources.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;

namespace web_app {

namespace {

const char kFakeChromeBundleId[] = "fake.cfbundleidentifier";

class WebAppShortcutCreatorMock : public WebAppShortcutCreator {
 public:
  WebAppShortcutCreatorMock(const base::FilePath& app_data_dir,
                            const ShortcutInfo* shortcut_info)
      : WebAppShortcutCreator(app_data_dir, shortcut_info) {}

  MOCK_CONST_METHOD0(GetAppBundlesByIdUnsorted, std::vector<base::FilePath>());
  MOCK_CONST_METHOD1(RevealAppShimInFinder, void(const base::FilePath&));

  WebAppShortcutCreatorMock(const WebAppShortcutCreatorMock&) = delete;
  WebAppShortcutCreatorMock& operator=(const WebAppShortcutCreatorMock&) =
      delete;
};

class WebAppShortcutCreatorSortingMock : public WebAppShortcutCreator {
 public:
  WebAppShortcutCreatorSortingMock(const base::FilePath& app_data_dir,
                                   const ShortcutInfo* shortcut_info)
      : WebAppShortcutCreator(app_data_dir, shortcut_info) {}

  MOCK_CONST_METHOD0(GetAppBundlesByIdUnsorted, std::vector<base::FilePath>());

  WebAppShortcutCreatorSortingMock(const WebAppShortcutCreatorSortingMock&) =
      delete;
  WebAppShortcutCreatorSortingMock& operator=(
      const WebAppShortcutCreatorSortingMock&) = delete;
};

class WebAppAutoLoginUtilMock : public WebAppAutoLoginUtil {
 public:
  WebAppAutoLoginUtilMock() = default;
  WebAppAutoLoginUtilMock(const WebAppAutoLoginUtilMock&) = delete;
  WebAppAutoLoginUtilMock& operator=(const WebAppAutoLoginUtilMock&) = delete;

  void AddToLoginItems(const base::FilePath& app_bundle_path,
                       bool hide_on_startup) override {
    EXPECT_TRUE(base::PathExists(app_bundle_path));
    EXPECT_FALSE(hide_on_startup);
    add_to_login_items_called_count_++;
  }

  void RemoveFromLoginItems(const base::FilePath& app_bundle_path) override {
    EXPECT_TRUE(base::PathExists(app_bundle_path));
    remove_from_login_items_called_count_++;
  }

  void ResetCounts() {
    add_to_login_items_called_count_ = 0;
    remove_from_login_items_called_count_ = 0;
  }

  int GetAddToLoginItemsCalledCount() const {
    return add_to_login_items_called_count_;
  }

  int GetRemoveFromLoginItemsCalledCount() const {
    return remove_from_login_items_called_count_;
  }

 private:
  int add_to_login_items_called_count_ = 0;
  int remove_from_login_items_called_count_ = 0;
};

std::unique_ptr<ShortcutInfo> GetShortcutInfo() {
  std::unique_ptr<ShortcutInfo> info(new ShortcutInfo);
  info->extension_id = "extensionid";
  info->title = u"Shortcut Title";
  info->url = GURL("http://example.com/");
  info->profile_path = base::FilePath("user_data_dir").Append("Profile 1");
  info->profile_name = "profile name";
  info->version_for_display = "stable 1.0";
  info->is_multi_profile = true;
  return info;
}

class WebAppShortcutCreatorTest : public testing::Test {
 protected:
  WebAppShortcutCreatorTest() {}
  WebAppShortcutCreatorTest(const WebAppShortcutCreatorTest&) = delete;
  WebAppShortcutCreatorTest& operator=(const WebAppShortcutCreatorTest&) =
      delete;

  void SetUp() override {
    base::mac::SetBaseBundleID(kFakeChromeBundleId);

    EXPECT_TRUE(temp_destination_dir_.CreateUniqueTempDir());
    EXPECT_TRUE(temp_user_data_dir_.CreateUniqueTempDir());
    destination_dir_ = temp_destination_dir_.GetPath();
    user_data_dir_ = temp_user_data_dir_.GetPath();
    // Recreate the directory structure as it would be created for the
    // ShortcutInfo created in the above GetShortcutInfo.
    app_data_dir_ = user_data_dir_.Append("Profile 1")
                        .Append("Web Applications")
                        .Append("_crx_extensionid");
    EXPECT_TRUE(base::CreateDirectory(app_data_dir_));

    // When using base::PathService::Override, it calls
    // base::MakeAbsoluteFilePath. On Mac this prepends "/private" to the path,
    // but points to the same directory in the file system.
    EXPECT_TRUE(
        base::PathService::Override(chrome::DIR_USER_DATA, user_data_dir_));
    user_data_dir_ = base::MakeAbsoluteFilePath(user_data_dir_);
    app_data_dir_ = base::MakeAbsoluteFilePath(app_data_dir_);

    SetChromeAppsFolderForTesting(destination_dir_);

    info_ = GetShortcutInfo();
    fallback_shim_base_name_ =
        base::FilePath(info_->profile_path.BaseName().value() + " " +
                       info_->extension_id + ".app");

    shim_base_name_ = base::FilePath(base::UTF16ToUTF8(info_->title) + ".app");
    shim_path_ = destination_dir_.Append(shim_base_name_);

    auto_login_util_mock_ = std::make_unique<WebAppAutoLoginUtilMock>();
    WebAppAutoLoginUtil::SetInstanceForTesting(auto_login_util_mock_.get());
  }

  void TearDown() override {
    WebAppAutoLoginUtil::SetInstanceForTesting(nullptr);
    SetChromeAppsFolderForTesting(base::FilePath());
    testing::Test::TearDown();
  }

  // Needed by DCHECK_CURRENTLY_ON in ShortcutInfo destructor.
  content::BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir temp_destination_dir_;
  base::ScopedTempDir temp_user_data_dir_;
  base::FilePath app_data_dir_;
  base::FilePath destination_dir_;
  base::FilePath user_data_dir_;

  std::unique_ptr<WebAppAutoLoginUtilMock> auto_login_util_mock_;
  std::unique_ptr<ShortcutInfo> info_;
  base::FilePath fallback_shim_base_name_;
  base::FilePath shim_base_name_;
  base::FilePath shim_path_;

};

}  // namespace

TEST_F(WebAppShortcutCreatorTest, CreateShortcuts) {
  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info_.get());
  base::FilePath strings_file =
      destination_dir_.Append(".localized").Append("en_US.strings");

  // The Chrome Apps folder shouldn't be localized yet.
  EXPECT_FALSE(base::PathExists(strings_file));

  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                               ShortcutLocations()));
  EXPECT_TRUE(base::PathExists(shim_path_));
  EXPECT_TRUE(base::PathExists(destination_dir_));
  EXPECT_EQ(shim_base_name_, shortcut_creator.GetShortcutBasename());

  // When a shortcut is created, the parent, "Chrome Apps" folder should become
  // localized, but only once, to avoid concurrency issues in NSWorkspace. Note
  // this will fail if the CreateShortcuts test is run multiple times in the
  // same process, but the test runner should never do that.
  EXPECT_TRUE(base::PathExists(strings_file));

  // Delete it here, just to test that it is not recreated.
  EXPECT_TRUE(base::DeletePathRecursively(strings_file));

  auto_login_util_mock_->ResetCounts();

  // Ensure the strings file wasn't recreated. It's not needed for any other
  // tests.
  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                               ShortcutLocations()));
  EXPECT_FALSE(base::PathExists(strings_file));
  EXPECT_EQ(auto_login_util_mock_->GetAddToLoginItemsCalledCount(), 0);

  base::FilePath plist_path =
      shim_path_.Append("Contents").Append("Info.plist");
  NSDictionary* plist = [NSDictionary
      dictionaryWithContentsOfFile:base::mac::FilePathToNSString(plist_path)];
  EXPECT_NSEQ(base::SysUTF8ToNSString(info_->extension_id),
              [plist objectForKey:app_mode::kCrAppModeShortcutIDKey]);
  EXPECT_NSEQ(base::SysUTF16ToNSString(info_->title),
              [plist objectForKey:app_mode::kCrAppModeShortcutNameKey]);
  EXPECT_NSEQ(base::SysUTF8ToNSString(info_->url.spec()),
              [plist objectForKey:app_mode::kCrAppModeShortcutURLKey]);

  EXPECT_NSEQ(base::SysUTF8ToNSString(version_info::GetVersionNumber()),
              [plist objectForKey:app_mode::kCrBundleVersionKey]);
  EXPECT_NSEQ(base::SysUTF8ToNSString(info_->version_for_display),
              [plist objectForKey:app_mode::kCFBundleShortVersionStringKey]);

  // Make sure all values in the plist are actually filled in.
  for (id key in plist) {
    id value = [plist valueForKey:key];
    if (!base::mac::ObjCCast<NSString>(value))
      continue;

    EXPECT_EQ(static_cast<NSUInteger>(NSNotFound),
              [value rangeOfString:@"@APP_"].location)
        << base::SysNSStringToUTF8(key) << ":"
        << base::SysNSStringToUTF8(value);
  }
}

TEST_F(WebAppShortcutCreatorTest, FileHandlers) {
  const base::FilePath plist_path =
      shim_path_.Append("Contents").Append("Info.plist");
  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info_.get());

  // kCFBundleDocumentTypesKey should not be set, because we set no file
  // handlers.
  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                               ShortcutLocations()));
  {
    NSDictionary* plist = [NSDictionary
        dictionaryWithContentsOfFile:base::mac::FilePathToNSString(plist_path)];
    NSArray* doc_types_array =
        [plist objectForKey:app_mode::kCFBundleDocumentTypesKey];
    EXPECT_EQ(doc_types_array, nil);
  }
  EXPECT_TRUE(base::DeletePathRecursively(shim_path_));

  // Register 2 mime types (and 2 invalid extensions). We should now have
  // kCFBundleTypeMIMETypesKey but not kCFBundleTypeExtensionsKey.
  info_->file_handler_extensions.insert("byobb");
  info_->file_handler_extensions.insert(".");
  info_->file_handler_mime_types.insert("foo/bar");
  info_->file_handler_mime_types.insert("moo/cow");
  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                               ShortcutLocations()));
  {
    NSDictionary* plist = [NSDictionary
        dictionaryWithContentsOfFile:base::mac::FilePathToNSString(plist_path)];
    NSArray* doc_types_array =
        [plist objectForKey:app_mode::kCFBundleDocumentTypesKey];
    EXPECT_NE(doc_types_array, nil);
    EXPECT_EQ(1u, [doc_types_array count]);
    NSDictionary* doc_types_dict = [doc_types_array objectAtIndex:0];
    EXPECT_NE(doc_types_dict, nil);
    NSArray* mime_types =
        [doc_types_dict objectForKey:app_mode::kCFBundleTypeMIMETypesKey];
    EXPECT_NE(mime_types, nil);
    NSArray* extensions =
        [doc_types_dict objectForKey:app_mode::kCFBundleTypeExtensionsKey];
    EXPECT_EQ(extensions, nil);

    // The mime types should be listed in sorted order (note that sorted order
    // does matter for correct behavior).
    EXPECT_EQ(2u, [mime_types count]);
    EXPECT_NSEQ([mime_types objectAtIndex:0], @"foo/bar");
    EXPECT_NSEQ([mime_types objectAtIndex:1], @"moo/cow");
  }
  EXPECT_TRUE(base::DeletePathRecursively(shim_path_));

  // Register 3 valid extensions (and 2 invalid ones) with the 2 mime types.
  info_->file_handler_extensions.insert(".cow");
  info_->file_handler_extensions.insert(".pig");
  info_->file_handler_extensions.insert(".bbq");
  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                               ShortcutLocations()));
  {
    NSDictionary* plist = [NSDictionary
        dictionaryWithContentsOfFile:base::mac::FilePathToNSString(plist_path)];
    NSArray* doc_types_array =
        [plist objectForKey:app_mode::kCFBundleDocumentTypesKey];
    EXPECT_NE(doc_types_array, nil);
    EXPECT_EQ(1u, [doc_types_array count]);
    NSDictionary* doc_types_dict = [doc_types_array objectAtIndex:0];
    EXPECT_NE(doc_types_dict, nil);
    NSArray* mime_types =
        [doc_types_dict objectForKey:app_mode::kCFBundleTypeMIMETypesKey];
    EXPECT_NE(mime_types, nil);
    NSArray* extensions =
        [doc_types_dict objectForKey:app_mode::kCFBundleTypeExtensionsKey];
    EXPECT_NE(extensions, nil);

    EXPECT_EQ(2u, [mime_types count]);
    EXPECT_NSEQ([mime_types objectAtIndex:0], @"foo/bar");
    EXPECT_NSEQ([mime_types objectAtIndex:1], @"moo/cow");
    EXPECT_EQ(3u, [extensions count]);
    EXPECT_NSEQ([extensions objectAtIndex:0], @"bbq");
    EXPECT_NSEQ([extensions objectAtIndex:1], @"cow");
    EXPECT_NSEQ([extensions objectAtIndex:2], @"pig");
  }
  EXPECT_TRUE(base::DeletePathRecursively(shim_path_));

  // Register just extensions.
  info_->file_handler_mime_types.clear();
  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                               ShortcutLocations()));
  {
    NSDictionary* plist = [NSDictionary
        dictionaryWithContentsOfFile:base::mac::FilePathToNSString(plist_path)];
    NSArray* doc_types_array =
        [plist objectForKey:app_mode::kCFBundleDocumentTypesKey];
    EXPECT_NE(doc_types_array, nil);
    EXPECT_EQ(1u, [doc_types_array count]);
    NSDictionary* doc_types_dict = [doc_types_array objectAtIndex:0];
    EXPECT_NE(doc_types_dict, nil);
    NSArray* mime_types =
        [doc_types_dict objectForKey:app_mode::kCFBundleTypeMIMETypesKey];
    EXPECT_EQ(mime_types, nil);
    NSArray* extensions =
        [doc_types_dict objectForKey:app_mode::kCFBundleTypeExtensionsKey];
    EXPECT_NE(extensions, nil);

    EXPECT_EQ(3u, [extensions count]);
    EXPECT_NSEQ([extensions objectAtIndex:0], @"bbq");
    EXPECT_NSEQ([extensions objectAtIndex:1], @"cow");
    EXPECT_NSEQ([extensions objectAtIndex:2], @"pig");
  }
  EXPECT_TRUE(base::DeletePathRecursively(shim_path_));
}

TEST_F(WebAppShortcutCreatorTest, ProtocolHandlers) {
  const base::FilePath plist_path =
      shim_path_.Append("Contents").Append("Info.plist");
  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info_.get());

  // CFBundleURLTypes should not be set, because we set no protocol
  // handlers.
  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                               ShortcutLocations()));
  {
    NSDictionary* plist = [NSDictionary
        dictionaryWithContentsOfFile:base::mac::FilePathToNSString(plist_path)];
    NSArray* protocol_types_value =
        [plist objectForKey:app_mode::kCFBundleURLTypesKey];
    EXPECT_EQ(protocol_types_value, nil);
  }
  EXPECT_TRUE(base::DeletePathRecursively(shim_path_));

  // Register 2 valid protocol handlers.
  info_->protocol_handlers.insert("mailto");
  info_->protocol_handlers.insert("web+testing");
  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                               ShortcutLocations()));
  {
    NSDictionary* plist = [NSDictionary
        dictionaryWithContentsOfFile:base::mac::FilePathToNSString(plist_path)];
    NSArray* protocol_types_value =
        [plist objectForKey:app_mode::kCFBundleURLTypesKey];
    EXPECT_NE(protocol_types_value, nil);
    EXPECT_EQ(1u, [protocol_types_value count]);
    NSDictionary* protocol_types_dict = [protocol_types_value objectAtIndex:0];
    EXPECT_NE(protocol_types_dict, nil);

    // Verify CFBundleURLName is set.
    EXPECT_NSEQ(
        [protocol_types_dict objectForKey:app_mode::kCFBundleURLNameKey],
        base::SysUTF8ToNSString(base::mac::BaseBundleID() +
                                std::string(".app.") + info_->extension_id));

    // Verify CFBundleURLSchemes is set, and contains the expected values.
    NSArray* handlers =
        [protocol_types_dict objectForKey:app_mode::kCFBundleURLSchemesKey];
    EXPECT_NE(handlers, nil);
    EXPECT_EQ(2u, [handlers count]);
    EXPECT_NSEQ([handlers objectAtIndex:0], @"mailto");
    EXPECT_NSEQ([handlers objectAtIndex:1], @"web+testing");
  }
  EXPECT_TRUE(base::DeletePathRecursively(shim_path_));
}

TEST_F(WebAppShortcutCreatorTest, CreateShortcutsConflict) {
  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info_.get());
  base::FilePath strings_file =
      destination_dir_.Append(".localized").Append("en_US.strings");

  // Create a conflicting .app.
  EXPECT_FALSE(base::PathExists(shim_path_));
  base::CreateDirectory(shim_path_);
  EXPECT_TRUE(base::PathExists(shim_path_));

  // Ensure that the " (2).app" path does not yet exist.
  base::FilePath conflict_base_name(base::UTF16ToUTF8(info_->title) + " 2.app");
  base::FilePath conflict_path = destination_dir_.Append(conflict_base_name);
  EXPECT_FALSE(base::PathExists(conflict_path));

  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                               ShortcutLocations()));

  // We should have created the " 2.app" path.
  EXPECT_TRUE(base::PathExists(conflict_path));
  EXPECT_TRUE(base::PathExists(destination_dir_));
}

TEST_F(WebAppShortcutCreatorTest, CreateShortcutsStartup) {
  WebAppShortcutCreatorMock shortcut_creator(app_data_dir_, info_.get());

  ShortcutLocations locations;
  locations.in_startup = true;

  auto_login_util_mock_->ResetCounts();
  EXPECT_FALSE(base::PathExists(shim_path_));
  EXPECT_CALL(shortcut_creator, RevealAppShimInFinder(_)).Times(0);
  EXPECT_TRUE(
      shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED, locations));
  EXPECT_TRUE(base::PathExists(shim_path_));
  EXPECT_EQ(auto_login_util_mock_->GetAddToLoginItemsCalledCount(), 1);
  EXPECT_TRUE(base::DeletePathRecursively(shim_path_));
}

TEST_F(WebAppShortcutCreatorTest, NormalizeTitle) {
  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info_.get());

  info_->title = u"../../Evil/";
  EXPECT_EQ(destination_dir_.Append(":..:Evil:.app"),
            shortcut_creator.GetApplicationsShortcutPath(false));

  info_->title = u"....";
  EXPECT_EQ(destination_dir_.Append(fallback_shim_base_name_),
            shortcut_creator.GetApplicationsShortcutPath(false));
}

TEST_F(WebAppShortcutCreatorTest, UpdateShortcuts) {
  base::ScopedTempDir other_folder_temp_dir;
  EXPECT_TRUE(other_folder_temp_dir.CreateUniqueTempDir());
  base::FilePath other_folder = other_folder_temp_dir.GetPath();
  base::FilePath other_shim_path = other_folder.Append(shim_base_name_);

  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info_.get());

  std::vector<base::FilePath> bundle_by_id_paths;
  bundle_by_id_paths.push_back(other_shim_path);
  EXPECT_CALL(shortcut_creator, GetAppBundlesByIdUnsorted())
      .WillOnce(Return(bundle_by_id_paths));

  EXPECT_TRUE(shortcut_creator.BuildShortcut(other_shim_path));

  EXPECT_TRUE(base::DeletePathRecursively(other_shim_path.Append("Contents")));

  std::vector<base::FilePath> updated_paths;
  EXPECT_TRUE(shortcut_creator.UpdateShortcuts(false, &updated_paths));
  EXPECT_FALSE(base::PathExists(shim_path_));
  EXPECT_TRUE(base::PathExists(other_shim_path.Append("Contents")));

  // The list of updated paths is the paths found by bundle.
  EXPECT_EQ(bundle_by_id_paths, updated_paths);

  // Also test case where GetAppBundlesByIdUnsorted fails.
  bundle_by_id_paths.clear();
  EXPECT_CALL(shortcut_creator, GetAppBundlesByIdUnsorted())
      .WillOnce(Return(bundle_by_id_paths));

  EXPECT_TRUE(shortcut_creator.BuildShortcut(other_shim_path));

  EXPECT_TRUE(base::DeletePathRecursively(other_shim_path.Append("Contents")));

  updated_paths.clear();
  EXPECT_FALSE(shortcut_creator.UpdateShortcuts(false, &updated_paths));
  EXPECT_TRUE(updated_paths.empty());
  EXPECT_FALSE(base::PathExists(shim_path_));
  EXPECT_FALSE(base::PathExists(other_shim_path.Append("Contents")));

  // Also test case where GetAppBundlesByIdUnsorted fails and recreation is
  // forced.
  bundle_by_id_paths.clear();
  EXPECT_CALL(shortcut_creator, GetAppBundlesByIdUnsorted())
      .WillOnce(Return(bundle_by_id_paths));

  // The default shim path is created along with the internal path.
  updated_paths.clear();
  EXPECT_TRUE(shortcut_creator.UpdateShortcuts(true, &updated_paths));
  EXPECT_EQ(1u, updated_paths.size());
  EXPECT_EQ(shim_path_, updated_paths[0]);
  EXPECT_TRUE(base::PathExists(shim_path_));
  EXPECT_FALSE(base::PathExists(other_shim_path.Append("Contents")));
}

TEST_F(WebAppShortcutCreatorTest, UpdateBookmarkAppShortcut) {
  base::ScopedTempDir other_folder_temp_dir;
  EXPECT_TRUE(other_folder_temp_dir.CreateUniqueTempDir());
  base::FilePath other_folder = other_folder_temp_dir.GetPath();
  base::FilePath other_shim_path = other_folder.Append(shim_base_name_);

  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info_.get());

  std::vector<base::FilePath> bundle_by_id_paths;
  bundle_by_id_paths.push_back(shim_path_);
  EXPECT_CALL(shortcut_creator, GetAppBundlesByIdUnsorted())
      .WillOnce(Return(bundle_by_id_paths));

  EXPECT_TRUE(shortcut_creator.BuildShortcut(other_shim_path));

  EXPECT_TRUE(base::DeletePathRecursively(other_shim_path));

  // The original shim should be recreated.
  std::vector<base::FilePath> updated_paths;
  EXPECT_TRUE(shortcut_creator.UpdateShortcuts(false, &updated_paths));
  EXPECT_TRUE(base::PathExists(shim_path_));
  EXPECT_FALSE(base::PathExists(other_shim_path.Append("Contents")));
}

TEST_F(WebAppShortcutCreatorTest, DeleteShortcutsSingleProfile) {
  info_->is_multi_profile = false;

  base::FilePath other_shim_path =
      shim_path_.DirName().Append("Copy of Shim.app");
  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info_.get());

  // Create an extra shim in another folder. It should be deleted since its
  // bundle id matches.
  std::vector<base::FilePath> bundle_by_id_paths;
  bundle_by_id_paths.push_back(shim_path_);
  bundle_by_id_paths.push_back(other_shim_path);
  EXPECT_CALL(shortcut_creator, GetAppBundlesByIdUnsorted())
      .WillRepeatedly(Return(bundle_by_id_paths));
  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                               ShortcutLocations()));

  // Ensure the paths were created, and that they are destroyed.
  EXPECT_TRUE(base::PathExists(shim_path_));
  EXPECT_TRUE(base::PathExists(other_shim_path));
  auto_login_util_mock_->ResetCounts();
  internals::DeleteMultiProfileShortcutsForApp(info_->extension_id);
  EXPECT_EQ(auto_login_util_mock_->GetRemoveFromLoginItemsCalledCount(), 0);

  EXPECT_TRUE(base::PathExists(shim_path_));
  EXPECT_TRUE(base::PathExists(other_shim_path));
  auto_login_util_mock_->ResetCounts();
  internals::DeletePlatformShortcuts(app_data_dir_, *info_);
  EXPECT_EQ(auto_login_util_mock_->GetRemoveFromLoginItemsCalledCount(), 2);
  EXPECT_FALSE(base::PathExists(shim_path_));
  EXPECT_FALSE(base::PathExists(other_shim_path));
}

TEST_F(WebAppShortcutCreatorTest, DeleteShortcuts) {
  base::FilePath other_shim_path =
      shim_path_.DirName().Append("Copy of Shim.app");
  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info_.get());

  // Create an extra shim in another folder. It should be deleted since its
  // bundle id matches.
  std::vector<base::FilePath> bundle_by_id_paths;
  bundle_by_id_paths.push_back(shim_path_);
  bundle_by_id_paths.push_back(other_shim_path);
  EXPECT_CALL(shortcut_creator, GetAppBundlesByIdUnsorted())
      .WillRepeatedly(Return(bundle_by_id_paths));
  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                               ShortcutLocations()));

  // Ensure the paths were created, and that they are destroyed.
  EXPECT_TRUE(base::PathExists(shim_path_));
  EXPECT_TRUE(base::PathExists(other_shim_path));
  auto_login_util_mock_->ResetCounts();
  internals::DeletePlatformShortcuts(app_data_dir_, *info_);
  EXPECT_EQ(auto_login_util_mock_->GetRemoveFromLoginItemsCalledCount(), 0);
  EXPECT_TRUE(base::PathExists(shim_path_));
  EXPECT_TRUE(base::PathExists(other_shim_path));
  auto_login_util_mock_->ResetCounts();
  internals::DeleteMultiProfileShortcutsForApp(info_->extension_id);
  EXPECT_EQ(auto_login_util_mock_->GetRemoveFromLoginItemsCalledCount(), 2);
  EXPECT_FALSE(base::PathExists(shim_path_));
  EXPECT_FALSE(base::PathExists(other_shim_path));
}

TEST_F(WebAppShortcutCreatorTest, DeleteAllShortcutsForProfile) {
  info_->is_multi_profile = false;

  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info_.get());
  base::FilePath profile_path = info_->profile_path;
  base::FilePath other_profile_path =
      profile_path.DirName().Append("Profile 2");

  EXPECT_FALSE(base::PathExists(shim_path_));
  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                               ShortcutLocations()));
  EXPECT_TRUE(base::PathExists(shim_path_));

  auto_login_util_mock_->ResetCounts();
  internals::DeleteAllShortcutsForProfile(other_profile_path);
  EXPECT_EQ(auto_login_util_mock_->GetRemoveFromLoginItemsCalledCount(), 0);
  EXPECT_TRUE(base::PathExists(shim_path_));

  auto_login_util_mock_->ResetCounts();
  internals::DeleteAllShortcutsForProfile(profile_path);
  EXPECT_EQ(auto_login_util_mock_->GetRemoveFromLoginItemsCalledCount(), 1);
  EXPECT_FALSE(base::PathExists(shim_path_));
}

TEST_F(WebAppShortcutCreatorTest, RunShortcut) {
  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info_.get());
  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                               ShortcutLocations()));
  EXPECT_TRUE(base::PathExists(shim_path_));

  ssize_t status = getxattr(shim_path_.value().c_str(), "com.apple.quarantine",
                            NULL, 0, 0, 0);
  EXPECT_EQ(-1, status);
  EXPECT_EQ(ENOATTR, errno);
}

TEST_F(WebAppShortcutCreatorTest, CreateFailure) {
  base::FilePath non_existent_path =
      destination_dir_.Append("not-existent").Append("name.app");
  SetChromeAppsFolderForTesting(non_existent_path);

  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info_.get());
  EXPECT_FALSE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                                ShortcutLocations()));
}

TEST_F(WebAppShortcutCreatorTest, UpdateIcon) {
  gfx::Image product_logo =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_PRODUCT_LOGO_32);
  info_->favicon.Add(product_logo);
  WebAppShortcutCreatorMock shortcut_creator(app_data_dir_, info_.get());

  ASSERT_TRUE(shortcut_creator.UpdateIcon(shim_path_));
  base::FilePath icon_path =
      shim_path_.Append("Contents").Append("Resources").Append("app.icns");

  base::scoped_nsobject<NSImage> image([[NSImage alloc]
      initWithContentsOfFile:base::mac::FilePathToNSString(icon_path)]);
  EXPECT_TRUE(image);
  EXPECT_EQ(product_logo.Width(), [image size].width);
  EXPECT_EQ(product_logo.Height(), [image size].height);
}

TEST_F(WebAppShortcutCreatorTest, RevealAppShimInFinder) {
  WebAppShortcutCreatorMock shortcut_creator(app_data_dir_, info_.get());

  EXPECT_CALL(shortcut_creator, RevealAppShimInFinder(_)).Times(0);
  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                               ShortcutLocations()));

  EXPECT_CALL(shortcut_creator, RevealAppShimInFinder(_));
  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_BY_USER,
                                               ShortcutLocations()));
}

TEST_F(WebAppShortcutCreatorTest, SortAppBundles) {
  base::FilePath app_dir("/home/apps");
  NiceMock<WebAppShortcutCreatorSortingMock> shortcut_creator(app_dir,
                                                              info_.get());
  base::FilePath a = shortcut_creator.GetApplicationsShortcutPath(false);
  base::FilePath b = GetChromeAppsFolder().Append("a");
  base::FilePath c = GetChromeAppsFolder().Append("z");
  base::FilePath d("/a/b/c");
  base::FilePath e("/z/y/w");
  std::vector<base::FilePath> unsorted = {e, c, a, d, b};
  std::vector<base::FilePath> sorted = {a, b, c, d, e};

  EXPECT_CALL(shortcut_creator, GetAppBundlesByIdUnsorted())
      .WillOnce(Return(unsorted));
  std::vector<base::FilePath> result = shortcut_creator.GetAppBundlesById();
  EXPECT_EQ(result, sorted);
}

TEST_F(WebAppShortcutCreatorTest, RemoveAppShimFromLoginItems) {
  WebAppShortcutCreatorMock shortcut_creator(app_data_dir_, info_.get());

  ShortcutLocations locations;
  locations.in_startup = true;

  auto_login_util_mock_->ResetCounts();
  EXPECT_FALSE(base::PathExists(shim_path_));
  EXPECT_CALL(shortcut_creator, RevealAppShimInFinder(_)).Times(0);
  EXPECT_TRUE(
      shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED, locations));
  EXPECT_TRUE(base::PathExists(shim_path_));
  EXPECT_EQ(auto_login_util_mock_->GetAddToLoginItemsCalledCount(), 1);

  auto_login_util_mock_->ResetCounts();
  RemoveAppShimFromLoginItems("does-not-exist-app");
  EXPECT_EQ(auto_login_util_mock_->GetRemoveFromLoginItemsCalledCount(), 0);

  auto_login_util_mock_->ResetCounts();
  RemoveAppShimFromLoginItems(info_->extension_id);
  EXPECT_EQ(auto_login_util_mock_->GetRemoveFromLoginItemsCalledCount(), 1);

  EXPECT_TRUE(base::DeletePathRecursively(shim_path_));
}

}  // namespace web_app
