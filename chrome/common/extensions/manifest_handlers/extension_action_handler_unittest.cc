// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/extension_action_handler.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "components/version_info/channel.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// TODO(devlin): We don't need this separate enum now that SystemIndicator is
// no longer part of ActionInfo.
enum class TestActionType {
  kBrowserAction,
  kPageAction,
  kAction,
};

base::FilePath GetTestDataDir() {
  base::FilePath path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &path);
  return path.AppendASCII("extensions").AppendASCII("manifest_handlers");
}

}  // namespace

// Tests that an unpacked extension with an invisible browser action
// default icon fails as expected.
TEST(ExtensionActionHandlerTest, LoadInvisibleBrowserActionIconUnpacked) {
  base::FilePath extension_dir =
      GetTestDataDir().AppendASCII("browser_action_invisible_icon");
  // Set the flag that enables the error.
  file_util::SetReportErrorForInvisibleIconForTesting(true);
  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      extension_dir, Manifest::UNPACKED, Extension::NO_FLAGS, &error));
  file_util::SetReportErrorForInvisibleIconForTesting(false);
  EXPECT_FALSE(extension);
  EXPECT_EQ("The icon is not sufficiently visible 'invisible_icon.png'.",
            error);
}

// Tests that an unpacked extension with an invisible page action
// default icon fails as expected.
TEST(ExtensionActionHandlerTest, LoadInvisiblePageActionIconUnpacked) {
  base::FilePath extension_dir =
      GetTestDataDir().AppendASCII("page_action_invisible_icon");
  // Set the flag that enables the error.
  file_util::SetReportErrorForInvisibleIconForTesting(true);
  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      extension_dir, Manifest::UNPACKED, Extension::NO_FLAGS, &error));
  file_util::SetReportErrorForInvisibleIconForTesting(false);
  EXPECT_FALSE(extension);
  EXPECT_EQ("The icon is not sufficiently visible 'invisible_icon.png'.",
            error);
}

// A parameterized test suite to test each different extension action key
// ("page_action", "browser_action", "action").
class ExtensionActionManifestTest
    : public ManifestTest,
      public testing::WithParamInterface<TestActionType> {
 public:
  ExtensionActionManifestTest() {}
  ~ExtensionActionManifestTest() override {}

  // Constructs and returns a ManifestData object with the provided
  // |action_spec|.
  ManifestData GetManifestData(const char* action_spec) {
    constexpr char kManifestStub[] =
        R"({
             "name": "Test",
             "manifest_version": 2,
             "version": "0.1",
             "%s": %s
           })";

    const char* action_key = nullptr;
    switch (GetParam()) {
      case TestActionType::kBrowserAction:
        action_key = manifest_keys::kBrowserAction;
        break;
      case TestActionType::kPageAction:
        action_key = manifest_keys::kPageAction;
        break;
      case TestActionType::kAction:
        action_key = manifest_keys::kAction;
        break;
    }

    base::Value manifest_value = base::test::ParseJson(
        base::StringPrintf(kManifestStub, action_key, action_spec));
    EXPECT_TRUE(manifest_value.is_dict());
    EXPECT_FALSE(manifest_value.is_none());
    return ManifestData(std::move(manifest_value), "test");
  }

  // Returns the ActionInfo for the given |extension| corresponding with the
  // action key of the test param.
  const ActionInfo* GetActionInfo(const Extension& extension) {
    const ActionInfo* action_info = nullptr;
    switch (GetParam()) {
      case TestActionType::kBrowserAction:
        action_info = ActionInfo::GetBrowserActionInfo(&extension);
        break;
      case TestActionType::kPageAction:
        action_info = ActionInfo::GetPageActionInfo(&extension);
        break;
      case TestActionType::kAction:
        action_info = ActionInfo::GetExtensionActionInfo(&extension);
        break;
    }

    return action_info;
  }

 private:
  // The "action" key is restricted to trunk.
  ScopedCurrentChannel scoped_channel_{version_info::Channel::UNKNOWN};

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionManifestTest);
};

// Tests that parsing an action succeeds and properly populates the given
// fields.
TEST_P(ExtensionActionManifestTest, Basic) {
  constexpr char kValidAllFields[] =
      R"({
           "default_popup": "popup.html",
           "default_title": "Title",
           "default_icon": "icon.png"
         })";
  scoped_refptr<const Extension> extension =
      LoadAndExpectSuccess(GetManifestData(kValidAllFields));
  ASSERT_TRUE(extension);
  const ActionInfo* action_info = GetActionInfo(*extension);
  ASSERT_TRUE(action_info);

  EXPECT_EQ(extension->GetResourceURL("popup.html"),
            action_info->default_popup_url);
  EXPECT_EQ("Title", action_info->default_title);
  // Make a copy of the map since [] is more readable than find() for comparing
  // values.
  ExtensionIconSet::IconMap icons = action_info->default_icon.map();
  EXPECT_EQ(1u, icons.size());
  EXPECT_EQ(icons[extension_misc::EXTENSION_ICON_GIGANTOR], "icon.png");
}

// Tests that specifying an empty action (e.g., "action": {}) works correctly,
// with empty defaults.
TEST_P(ExtensionActionManifestTest, TestEmptyAction) {
  constexpr char kValidNoFields[] = "{}";
  scoped_refptr<const Extension> extension =
      LoadAndExpectSuccess(GetManifestData(kValidNoFields));
  ASSERT_TRUE(extension);
  const ActionInfo* action_info = GetActionInfo(*extension);
  ASSERT_TRUE(action_info);

  EXPECT_EQ(GURL(), action_info->default_popup_url);
  EXPECT_EQ(std::string(), action_info->default_title);
  const ExtensionIconSet::IconMap& icons = action_info->default_icon.map();
  EXPECT_TRUE(icons.empty());
}

// Tests specifying an icon dictionary (with different pixel sizes) in the
// action.
TEST_P(ExtensionActionManifestTest, ValidIconDictionary) {
  constexpr char kValidIconDictionary[] =
      R"({
           "default_icon": {
             "24": "icon24.png",
             "48": "icon48.png",
             "79": "icon79.png"
           }
         })";

  scoped_refptr<const Extension> extension =
      LoadAndExpectSuccess(GetManifestData(kValidIconDictionary));
  ASSERT_TRUE(extension);
  const ActionInfo* action_info = GetActionInfo(*extension);
  ASSERT_TRUE(action_info);

  EXPECT_EQ(GURL(), action_info->default_popup_url);
  EXPECT_EQ(std::string(), action_info->default_title);
  // Make a copy of the map since [] is more readable than find() for comparing
  // values.
  ExtensionIconSet::IconMap icons = action_info->default_icon.map();
  EXPECT_EQ(3u, icons.size());
  EXPECT_EQ("icon24.png", icons[24]);
  EXPECT_EQ("icon48.png", icons[48]);
  EXPECT_EQ("icon79.png", icons[79]);
}

// Tests some invalid cases.
TEST_P(ExtensionActionManifestTest, Invalid) {
  constexpr char kInvalidTopLevel1[] = "[]";
  constexpr char kInvalidTopLevel2[] = "\"foo\"";
  constexpr char kInvalidTopLevel3[] = "17";

  const char* expected_error = nullptr;
  switch (GetParam()) {
    case TestActionType::kBrowserAction:
      expected_error = manifest_errors::kInvalidBrowserAction;
      break;
    case TestActionType::kPageAction:
      expected_error = manifest_errors::kInvalidPageAction;
      break;
    case TestActionType::kAction:
      expected_error = manifest_errors::kInvalidAction;
      break;
  }

  for (const char* spec :
       {kInvalidTopLevel1, kInvalidTopLevel2, kInvalidTopLevel3}) {
    LoadAndExpectError(GetManifestData(spec), expected_error);
  }

  constexpr char kInvalidPopup[] = R"({ "default_popup": {} })";
  LoadAndExpectError(GetManifestData(kInvalidPopup),
                     manifest_errors::kInvalidActionDefaultPopup);
  constexpr char kInvalidTitle[] = R"({ "default_title": {} })";
  LoadAndExpectError(GetManifestData(kInvalidTitle),
                     manifest_errors::kInvalidActionDefaultTitle);
  constexpr char kInvalidIcon[] = R"({ "default_icon": [] })";
  LoadAndExpectError(GetManifestData(kInvalidIcon),
                     manifest_errors::kInvalidActionDefaultIcon);
}

// Test the handling of the default_state key.
TEST_P(ExtensionActionManifestTest, DefaultState) {
  constexpr char kDefaultStateDisabled[] = R"({"default_state": "disabled"})";
  constexpr char kDefaultStateEnabled[] = R"({"default_state": "enabled"})";
  constexpr char kDefaultStateInvalid[] = R"({"default_state": "foo"})";

  // default_state is only valid for "action" types.
  const bool default_state_allowed = GetParam() == TestActionType::kAction;
  const char* key_disallowed_error =
      manifest_errors::kDefaultStateShouldNotBeSet;

  struct {
    // The manifest definition of the action key.
    const char* spec;
    // The expected error, if parsing was unsuccessful.
    const char* expected_error;
    // The expected state, if parsing was successful.
    base::Optional<ActionInfo::DefaultState> expected_state;
  } test_cases[] = {
      {kDefaultStateDisabled,
       default_state_allowed ? nullptr : key_disallowed_error,
       default_state_allowed ? base::make_optional(ActionInfo::STATE_DISABLED)
                             : base::nullopt},
      {kDefaultStateEnabled,
       default_state_allowed ? nullptr : key_disallowed_error,
       default_state_allowed ? base::make_optional(ActionInfo::STATE_ENABLED)
                             : base::nullopt},
      {kDefaultStateInvalid,
       default_state_allowed ? manifest_errors::kInvalidActionDefaultState
                             : key_disallowed_error,
       base::nullopt},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.spec);

    if (test_case.expected_error == nullptr) {
      ASSERT_TRUE(test_case.expected_state);
      scoped_refptr<const Extension> extension =
          LoadAndExpectSuccess(GetManifestData(test_case.spec));
      ASSERT_TRUE(extension);
      const ActionInfo* action_info = GetActionInfo(*extension);
      ASSERT_TRUE(action_info);
      EXPECT_EQ(*test_case.expected_state, action_info->default_state);
    } else {
      ASSERT_FALSE(test_case.expected_state);
      LoadAndExpectError(GetManifestData(test_case.spec),
                         test_case.expected_error);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         ExtensionActionManifestTest,
                         testing::Values(TestActionType::kBrowserAction,
                                         TestActionType::kPageAction,
                                         TestActionType::kAction));

}  // namespace extensions
