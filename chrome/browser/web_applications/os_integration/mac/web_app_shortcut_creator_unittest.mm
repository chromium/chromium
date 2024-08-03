// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_creator.h"

#import <Cocoa/Cocoa.h>
#include <errno.h>
#include <stddef.h>
#include <sys/xattr.h>

#include <memory>
#include <optional>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/web_applications/os_integration/mac/apps_folder_support.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_auto_login_util.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
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
      : WebAppShortcutCreator(app_data_dir,
                              GetChromeAppsFolder(),
                              shortcut_info,
                              web_app::UseAdHocSigningForWebAppShims()) {}

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
      : WebAppShortcutCreator(app_data_dir,
                              GetChromeAppsFolder(),
                              shortcut_info,
                              web_app::UseAdHocSigningForWebAppShims()) {}

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
  info->app_id = "appid";
  info->title = u"Shortcut Title";
  info->url = GURL("http://example.com/");
  info->profile_path = base::FilePath("user_data_dir").Append("Profile 1");
  info->profile_name = "profile name";
  info->version_for_display = "stable 1.0";
  info->is_multi_profile = true;
  return info;
}

class WebAppShortcutCreatorTest : public testing::Test {
 public:
  WebAppShortcutCreatorTest(const WebAppShortcutCreatorTest&) = delete;
  WebAppShortcutCreatorTest& operator=(const WebAppShortcutCreatorTest&) =
      delete;

 protected:
  WebAppShortcutCreatorTest() = default;

  void SetUp() override {
    base::apple::SetBaseBundleID(kFakeChromeBundleId);

    override_registration_ =
        OsIntegrationTestOverrideImpl::OverrideForTesting();
    destination_dir_ =
        override_registration_->test_override().chrome_apps_folder();

    EXPECT_TRUE(temp_user_data_dir_.CreateUniqueTempDir());
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
    user_data_dir_override_.emplace(chrome::DIR_USER_DATA, user_data_dir_);
    user_data_dir_ = base::MakeAbsoluteFilePath(user_data_dir_);
    app_data_dir_ = base::MakeAbsoluteFilePath(app_data_dir_);

    info_ = GetShortcutInfo();
    fallback_shim_base_name_ = base::FilePath(
        info_->profile_path.BaseName().value() + " " + info_->app_id + ".app");

    shim_base_name_ = base::FilePath(base::UTF16ToUTF8(info_->title) + ".app");
    shim_path_ = destination_dir_.Append(shim_base_name_);

    auto_login_util_mock_ = std::make_unique<WebAppAutoLoginUtilMock>();
    WebAppAutoLoginUtil::SetInstanceForTesting(auto_login_util_mock_.get());

    // Make sure that the tests in this class will actually try to set the
    // localized app dir name.
    ResetHaveLocalizedAppDirNameForTesting();
  }

  void TearDown() override {
    WebAppAutoLoginUtil::SetInstanceForTesting(nullptr);
    override_registration_.reset();
    testing::Test::TearDown();
  }

  // Needed by DCHECK_CURRENTLY_ON in ShortcutInfo destructor.
  content::BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir temp_user_data_dir_;
  base::FilePath app_data_dir_;
  base::FilePath destination_dir_;
  base::FilePath user_data_dir_;
  std::optional<base::ScopedPathOverride> user_data_dir_override_;

  std::unique_ptr<WebAppAutoLoginUtilMock> auto_login_util_mock_;
  std::unique_ptr<ShortcutInfo> info_;
  base::FilePath fallback_shim_base_name_;
  base::FilePath shim_base_name_;
  base::FilePath shim_path_;

  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
};

}  // namespace

TEST_F(WebAppShortcutCreatorTest, CreateShortcuts) {
  base::FilePath strings_file =
      destination_dir_.Append(".localized").Append("en_US.strings");

  // The Chrome Apps folder shouldn't be localized yet.
  EXPECT_FALSE(base::PathExists(strings_file));

  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info_.get());

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
      dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(plist_path)
                            error:nil];
  EXPECT_NSEQ(base::SysUTF8ToNSString(info_->app_id),
              plist[app_mode::kCrAppModeShortcutIDKey]);
  EXPECT_NSEQ(base::SysUTF16ToNSString(info_->title),
              plist[app_mode::kCrAppModeShortcutNameKey]);
  EXPECT_NSEQ(base::SysUTF8ToNSString(info_->url.spec()),
              plist[app_mode::kCrAppModeShortcutURLKey]);

  EXPECT_NSEQ(base::SysUTF8ToNSString(version_info::GetVersionNumber()),
              plist[app_mode::kCrBundleVersionKey]);
  EXPECT_NSEQ(base::SysUTF8ToNSString(info_->version_for_display),
              plist[app_mode::kCFBundleShortVersionStringKey]);

  // Make sure all values in the plist are actually filled in.
  for (id key in plist) {
    id value = [plist valueForKey:key];
    if (!base::apple::ObjCCast<NSString>(value)) {
      continue;
    }

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
        dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(plist_path)
                              error:nil];
    NSArray* doc_types_array = plist[app_mode::kCFBundleDocumentTypesKey];
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
        dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(plist_path)
                              error:nil];
    NSArray* doc_types_array = plist[app_mode::kCFBundleDocumentTypesKey];
    EXPECT_NE(doc_types_array, nil);
    EXPECT_EQ(1u, [doc_types_array count]);
    NSDictionary* doc_types_dict = doc_types_array[0];
    EXPECT_NE(doc_types_dict, nil);
    NSArray* mime_types = doc_types_dict[app_mode::kCFBundleTypeMIMETypesKey];
    EXPECT_NE(mime_types, nil);
    NSArray* extensions = doc_types_dict[app_mode::kCFBundleTypeExtensionsKey];
    EXPECT_EQ(extensions, nil);

    // The mime types should be listed in sorted order (note that sorted order
    // does matter for correct behavior).
    EXPECT_EQ(2u, [mime_types count]);
    EXPECT_NSEQ(mime_types[0], @"foo/bar");
    EXPECT_NSEQ(mime_types[1], @"moo/cow");
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
        dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(plist_path)
                              error:nil];
    NSArray* doc_types_array = plist[app_mode::kCFBundleDocumentTypesKey];
    EXPECT_NE(doc_types_array, nil);
    EXPECT_EQ(1u, [doc_types_array count]);
    NSDictionary* doc_types_dict = doc_types_array[0];
    EXPECT_NE(doc_types_dict, nil);
    NSArray* mime_types = doc_types_dict[app_mode::kCFBundleTypeMIMETypesKey];
    EXPECT_NE(mime_types, nil);
    NSArray* extensions = doc_types_dict[app_mode::kCFBundleTypeExtensionsKey];
    EXPECT_NE(extensions, nil);

    EXPECT_EQ(2u, [mime_types count]);
    EXPECT_NSEQ(mime_types[0], @"foo/bar");
    EXPECT_NSEQ(mime_types[1], @"moo/cow");
    EXPECT_EQ(3u, [extensions count]);
    EXPECT_NSEQ(extensions[0], @"bbq");
    EXPECT_NSEQ(extensions[1], @"cow");
    EXPECT_NSEQ(extensions[2], @"pig");
  }
  EXPECT_TRUE(base::DeletePathRecursively(shim_path_));

  // Register just extensions.
  info_->file_handler_mime_types.clear();
  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                               ShortcutLocations()));
  {
    NSDictionary* plist = [NSDictionary
        dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(plist_path)
                              error:nil];
    NSArray* doc_types_array = plist[app_mode::kCFBundleDocumentTypesKey];
    EXPECT_NE(doc_types_array, nil);
    EXPECT_EQ(1u, [doc_types_array count]);
    NSDictionary* doc_types_dict = doc_types_array[0];
    EXPECT_NE(doc_types_dict, nil);
    NSArray* mime_types = doc_types_dict[app_mode::kCFBundleTypeMIMETypesKey];
    EXPECT_EQ(mime_types, nil);
    NSArray* extensions = doc_types_dict[app_mode::kCFBundleTypeExtensionsKey];
    EXPECT_NE(extensions, nil);

    EXPECT_EQ(3u, [extensions count]);
    EXPECT_NSEQ(extensions[0], @"bbq");
    EXPECT_NSEQ(extensions[1], @"cow");
    EXPECT_NSEQ(extensions[2], @"pig");
  }
  EXPECT_TRUE(base::DeletePathRecursively(shim_path_));

  // Register extensions and mime types in a separate profile.
  const base::FilePath profile1 =
      base::FilePath("user_data_dir").Append("Profile 1");
  const base::FilePath profile2 =
      base::FilePath("user_data_dir").Append("Profile 2");
  info_->file_handler_extensions.clear();
  info_->handlers_per_profile[profile1].file_handler_extensions = {".cow"};
  info_->handlers_per_profile[profile2].file_handler_extensions = {
      ".bbq", ".pig", ".", "byobb"};
  info_->handlers_per_profile[profile1].file_handler_mime_types = {"foo/bar"};
  info_->handlers_per_profile[profile2].file_handler_mime_types = {"moo/cow"};
  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                               ShortcutLocations()));
  {
    NSDictionary* plist = [NSDictionary
        dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(plist_path)
                              error:nil];
    NSArray* doc_types_array = plist[app_mode::kCFBundleDocumentTypesKey];
    EXPECT_NE(doc_types_array, nil);
    EXPECT_EQ(1u, [doc_types_array count]);
    NSDictionary* doc_types_dict = doc_types_array[0];
    EXPECT_NE(doc_types_dict, nil);
    NSArray* mime_types = doc_types_dict[app_mode::kCFBundleTypeMIMETypesKey];
    EXPECT_NE(mime_types, nil);
    NSArray* extensions = doc_types_dict[app_mode::kCFBundleTypeExtensionsKey];
    EXPECT_NE(extensions, nil);

    EXPECT_EQ(2u, [extensions count]);
    EXPECT_NSEQ(extensions[0], @"bbq");
    EXPECT_NSEQ(extensions[1], @"pig");
    EXPECT_EQ(1u, [mime_types count]);
    EXPECT_NSEQ(mime_types[0], @"moo/cow");
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
        dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(plist_path)
                              error:nil];
    NSArray* protocol_types_value = plist[app_mode::kCFBundleURLTypesKey];
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
        dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(plist_path)
                              error:nil];
    NSArray* protocol_types_value = plist[app_mode::kCFBundleURLTypesKey];
    EXPECT_NE(protocol_types_value, nil);
    EXPECT_EQ(1u, [protocol_types_value count]);
    NSDictionary* protocol_types_dict = protocol_types_value[0];
    EXPECT_NE(protocol_types_dict, nil);

    // Verify CFBundleURLName is set.
    EXPECT_NSEQ(protocol_types_dict[app_mode::kCFBundleURLNameKey],
                base::SysUTF8ToNSString(base::apple::BaseBundleID() +
                                        std::string(".app.") + info_->app_id));

    // Verify CFBundleURLSchemes is set, and contains the expected values.
    NSArray* handlers = protocol_types_dict[app_mode::kCFBundleURLSchemesKey];
    EXPECT_NE(handlers, nil);
    EXPECT_EQ(2u, [handlers count]);
    EXPECT_NSEQ(handlers[0], @"mailto");
    EXPECT_NSEQ(handlers[1], @"web+testing");
  }
  EXPECT_TRUE(base::DeletePathRecursively(shim_path_));

  // Register protocol handlers in a separate profile.
  const base::FilePath profile1 =
      base::FilePath("user_data_dir").Append("Profile 1");
  const base::FilePath profile2 =
      base::FilePath("user_data_dir").Append("Profile 2");
  info_->protocol_handlers.clear();
  info_->handlers_per_profile[profile1].protocol_handlers = {"mailto"};
  info_->handlers_per_profile[profile2].protocol_handlers = {"web+testing"};
  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                               ShortcutLocations()));
  {
    NSDictionary* plist = [NSDictionary
        dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(plist_path)
                              error:nil];
    NSArray* protocol_types_value = plist[app_mode::kCFBundleURLTypesKey];
    EXPECT_NE(protocol_types_value, nil);
    EXPECT_EQ(1u, [protocol_types_value count]);
    NSDictionary* protocol_types_dict = protocol_types_value[0];
    EXPECT_NE(protocol_types_dict, nil);

    // Verify CFBundleURLName is set.
    EXPECT_NSEQ(protocol_types_dict[app_mode::kCFBundleURLNameKey],
                base::SysUTF8ToNSString(base::apple::BaseBundleID() +
                                        std::string(".app.") + info_->app_id));

    // Verify CFBundleURLSchemes is set, and contains the expected values.
    NSArray* handlers = protocol_types_dict[app_mode::kCFBundleURLSchemesKey];
    EXPECT_NE(handlers, nil);
    EXPECT_EQ(1u, [handlers count]);
    EXPECT_NSEQ(handlers[0], @"web+testing");
  }
  EXPECT_TRUE(base::DeletePathRecursively(shim_path_));
}

TEST_F(WebAppShortcutCreatorTest, CreateShortcutsConflict) {
  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info_.get());

  // Create a conflicting .app.
  EXPECT_FALSE(base::PathExists(shim_path_));
  base::CreateDirectory(shim_path_);
  EXPECT_TRUE(base::PathExists(shim_path_));

  // Ensure that the " 1.app" path does not yet exist.
  base::FilePath conflict_base_name(base::UTF16ToUTF8(info_->title) + " 1.app");
  base::FilePath conflict_path = destination_dir_.Append(conflict_base_name);
  EXPECT_FALSE(base::PathExists(conflict_path));

  EXPECT_TRUE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                               ShortcutLocations()));

  // We should have created the " 1.app" path.
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
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath update_folder = temp_dir.GetPath();
  base::FilePath other_shim_path = update_folder.Append(shim_base_name_);

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
  EXPECT_TRUE(shortcut_creator.UpdateShortcuts(false, &updated_paths));
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

TEST_F(WebAppShortcutCreatorTest, UpdateShortcutsWithTitleChange) {
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath update_folder = temp_dir.GetPath();
  base::FilePath shim_path = update_folder.Append(shim_base_name_);

  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info_.get());

  std::vector<base::FilePath> bundle_by_id_paths;
  bundle_by_id_paths.push_back(shim_path);
  EXPECT_CALL(shortcut_creator, GetAppBundlesByIdUnsorted())
      .WillOnce(Return(bundle_by_id_paths));

  EXPECT_TRUE(shortcut_creator.BuildShortcut(shim_path));

  // After building, the bundle should exist and have the right name.
  EXPECT_TRUE(base::PathExists(
      update_folder.Append("Shortcut Title.app").Append("Contents")));

  // The bundle name (in Info.plist) should also be set to 'Shortcut Title'.
  base::FilePath plist_path = update_folder.Append("Shortcut Title.app")
                                  .Append("Contents")
                                  .Append("Info.plist");
  EXPECT_TRUE(base::PathExists(plist_path));
  NSDictionary* plist = [NSDictionary
      dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(plist_path)
                            error:nil];
  EXPECT_NSEQ(@"Shortcut Title",
              plist[base::apple::CFToNSPtrCast(kCFBundleNameKey)]);

  // The display name (in InfoPlist.strings) should also be 'Shortcut Title'.
  NSString* language = [NSLocale preferredLanguages][0];
  std::string locale_dir_name = base::SysNSStringToUTF8(language) + ".lproj";
  base::FilePath resource_file_path = plist_path.DirName()
                                          .Append("Resources")
                                          .Append(locale_dir_name)
                                          .Append("InfoPlist.strings");
  EXPECT_TRUE(base::PathExists(resource_file_path));
  NSDictionary* resources =
      [NSDictionary dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(
                                                    resource_file_path)
                                          error:nil];
  EXPECT_NSEQ(@"Shortcut Title", resources[app_mode::kCFBundleDisplayNameKey]);

  // UpdateShortcuts does this as well, but clear the app bundle contents to
  // ensure expectations are testing against new data. Keep the top-level path
  // to ensure UpdateShortcuts detects its presence.
  EXPECT_TRUE(base::DeletePathRecursively(shim_path.Append("Contents")));

  std::vector<base::FilePath> updated_paths;
  EXPECT_TRUE(shortcut_creator.UpdateShortcuts(false, &updated_paths));

  EXPECT_EQ(
      std::vector<base::FilePath>{update_folder.Append("Shortcut Title.app")},
      updated_paths);

  // After updating, the bundle should still exist and have the same name.
  EXPECT_TRUE(base::PathExists(
      update_folder.Append("Shortcut Title.app").Append("Contents")));

  // The bundle name (in Info.plist) should still be set to 'Shortcut Title'.
  plist_path = update_folder.Append("Shortcut Title.app")
                   .Append("Contents")
                   .Append("Info.plist");
  EXPECT_TRUE(base::PathExists(plist_path));
  plist = [NSDictionary
      dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(plist_path)
                            error:nil];
  EXPECT_NSEQ(@"Shortcut Title",
              plist[base::apple::CFToNSPtrCast(kCFBundleNameKey)]);

  // The display name (in InfoPlist.strings) should still be 'Shortcut Title'.
  resource_file_path = plist_path.DirName()
                           .Append("Resources")
                           .Append(locale_dir_name)
                           .Append("InfoPlist.strings");
  EXPECT_TRUE(base::PathExists(resource_file_path));
  resources =
      [NSDictionary dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(
                                                    resource_file_path)
                                          error:nil];
  EXPECT_NSEQ(@"Shortcut Title", resources[app_mode::kCFBundleDisplayNameKey]);

  // Now simulate an update with a different title.
  std::unique_ptr<ShortcutInfo> info = GetShortcutInfo();
  info->title = u"New App Title";
  NiceMock<WebAppShortcutCreatorMock> shortcut_creator2(app_data_dir_,
                                                        info.get());

  EXPECT_CALL(shortcut_creator2, GetAppBundlesByIdUnsorted())
      .WillOnce(Return(bundle_by_id_paths));

  updated_paths.clear();
  EXPECT_TRUE(shortcut_creator2.UpdateShortcuts(false, &updated_paths));

  EXPECT_EQ(
      std::vector<base::FilePath>{update_folder.Append("Shortcut Title.app")},
      updated_paths);

  // After updating, the bundle should not have changed its name. Note that this
  // assumes an entirely new bundle wasn't created, which is verified by
  // expectations below.
  EXPECT_TRUE(base::PathExists(
      update_folder.Append("Shortcut Title.app").Append("Contents")));

  // The bundle name (in Info.plist) should also still be set to 'Shortcut
  // Title'.
  plist_path = update_folder.Append("Shortcut Title.app")
                   .Append("Contents")
                   .Append("Info.plist");
  EXPECT_TRUE(base::PathExists(plist_path));
  plist = [NSDictionary
      dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(plist_path)
                            error:nil];
  EXPECT_NSEQ(@"Shortcut Title",
              plist[base::apple::CFToNSPtrCast(kCFBundleNameKey)]);

  // The display name (in InfoPlist.strings) should have changed to 'New App
  // Title'.
  resource_file_path = plist_path.DirName()
                           .Append("Resources")
                           .Append(locale_dir_name)
                           .Append("InfoPlist.strings");
  EXPECT_TRUE(base::PathExists(resource_file_path));
  resources =
      [NSDictionary dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(
                                                    resource_file_path)
                                          error:nil];
  EXPECT_NSEQ(@"New App Title", resources[app_mode::kCFBundleDisplayNameKey]);
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

TEST_F(WebAppShortcutCreatorTest, NormalizeColonsInDisplayName) {
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath update_folder = temp_dir.GetPath();
  base::FilePath shim_path = update_folder.Append("App Title: New.app");

  std::unique_ptr<ShortcutInfo> info = GetShortcutInfo();
  info->title = u"App Title: New";
  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info.get());

  std::vector<base::FilePath> bundle_by_id_paths;
  bundle_by_id_paths.push_back(shim_path);
  EXPECT_CALL(shortcut_creator, GetAppBundlesByIdUnsorted())
      .WillOnce(Return(bundle_by_id_paths));

  EXPECT_TRUE(shortcut_creator.BuildShortcut(shim_path));

  std::vector<base::FilePath> updated_paths;
  EXPECT_TRUE(shortcut_creator.UpdateShortcuts(false, &updated_paths));

  // After building, the bundle should exist and have the right name.
  EXPECT_TRUE(base::PathExists(
      update_folder.Append("App Title: New.app").Append("Contents")));

  // The bundle name (in Info.plist) should also match.
  base::FilePath plist_path = update_folder.Append("App Title: New.app")
                                  .Append("Contents")
                                  .Append("Info.plist");
  EXPECT_TRUE(base::PathExists(plist_path));
  NSDictionary* plist = [NSDictionary
      dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(plist_path)
                            error:nil];
  EXPECT_NSEQ(@"App Title: New",
              plist[base::apple::CFToNSPtrCast(kCFBundleNameKey)]);

  // The display name (in InfoPlist.strings) should not have the colon.
  NSString* language = [NSLocale preferredLanguages][0];
  std::string locale_dir_name = base::SysNSStringToUTF8(language) + ".lproj";
  base::FilePath resource_file_path = plist_path.DirName()
                                          .Append("Resources")
                                          .Append(locale_dir_name)
                                          .Append("InfoPlist.strings");
  EXPECT_TRUE(base::PathExists(resource_file_path));
  NSDictionary* resources =
      [NSDictionary dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(
                                                    resource_file_path)
                                          error:nil];
  EXPECT_NSEQ(@"App Title New", resources[app_mode::kCFBundleDisplayNameKey]);
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
  internals::DeleteMultiProfileShortcutsForApp(info_->app_id);
  EXPECT_EQ(auto_login_util_mock_->GetRemoveFromLoginItemsCalledCount(), 0);

  EXPECT_TRUE(base::PathExists(shim_path_));
  EXPECT_TRUE(base::PathExists(other_shim_path));
  auto_login_util_mock_->ResetCounts();

  internals::DeletePlatformShortcuts(
      app_data_dir_, *info_, task_environment_.GetMainThreadTaskRunner(),
      base::DoNothing());

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
  internals::DeletePlatformShortcuts(
      app_data_dir_, *info_, task_environment_.GetMainThreadTaskRunner(),
      base::DoNothing());
  EXPECT_EQ(auto_login_util_mock_->GetRemoveFromLoginItemsCalledCount(), 0);
  EXPECT_TRUE(base::PathExists(shim_path_));
  EXPECT_TRUE(base::PathExists(other_shim_path));
  auto_login_util_mock_->ResetCounts();
  internals::DeleteMultiProfileShortcutsForApp(info_->app_id);
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

  ssize_t status =
      getxattr(shim_path_.value().c_str(), "com.apple.quarantine",
               /*value=*/nullptr, /*size=*/0, /*position=*/0, /*options=*/0);
  EXPECT_EQ(-1, status);
  EXPECT_EQ(ENOATTR, errno);
}

TEST_F(WebAppShortcutCreatorTest, CreateFailure) {
  ASSERT_TRUE(override_registration_->test_override().DeleteChromeAppsDir());

  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info_.get());
  EXPECT_FALSE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                                ShortcutLocations()));
}

TEST_F(WebAppShortcutCreatorTest, UpdateIcon) {
  gfx::Image product_logo_16 =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_PRODUCT_LOGO_16);
  gfx::Image product_logo_32 =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_PRODUCT_LOGO_32);

  WebAppShortcutCreatorMock shortcut_creator(app_data_dir_, info_.get());
  base::FilePath icon_path =
      shim_path_.Append("Contents").Append("Resources").Append("app.icns");

  // regular favicon should be used if no maskable favicons exist
  info_->favicon.Add(product_logo_32);
  ASSERT_TRUE(shortcut_creator.UpdateIcon(shim_path_));
  NSImage* image = [[NSImage alloc]
      initWithContentsOfFile:base::apple::FilePathToNSString(icon_path)];
  EXPECT_TRUE(image);
  EXPECT_EQ(product_logo_32.Width(), image.size.width);
  EXPECT_EQ(product_logo_32.Height(), image.size.height);

  // maskable favicon should be used if present
  info_->favicon_maskable.Add(product_logo_16);
  ASSERT_TRUE(shortcut_creator.UpdateIcon(shim_path_));
  image = [[NSImage alloc]
      initWithContentsOfFile:base::apple::FilePathToNSString(icon_path)];
  EXPECT_TRUE(image);
  EXPECT_EQ(product_logo_16.Width(), image.size.width);
  EXPECT_EQ(product_logo_16.Height(), image.size.height);
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
  RemoveAppShimFromLoginItems(info_->app_id);
  EXPECT_EQ(auto_login_util_mock_->GetRemoveFromLoginItemsCalledCount(), 1);

  EXPECT_TRUE(base::DeletePathRecursively(shim_path_));
}

}  // namespace web_app
