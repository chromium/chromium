// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/reload_page_dialog_controller.h"

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

class ReloadPageDialogControllerAndroidBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  ReloadPageDialogControllerAndroidBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsMenuAccessControl);
  }

  void SetUp() override {
    extensions::ExtensionBrowserTest::SetUp();
    message_dispatcher_bridge_.SetMessagesEnabledForEmbedder(true);
    messages::MessageDispatcherBridge::SetInstanceForTesting(
        &message_dispatcher_bridge_);
  }
  void TearDown() override {
    reload_page_dialog_.reset();
    messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
    extensions::ExtensionBrowserTest::TearDown();
  }

 protected:
  messages::MockMessageDispatcherBridge* message_dispatcher_bridge() {
    return &message_dispatcher_bridge_;
  }
  std::unique_ptr<extensions::ReloadPageDialogController> reload_page_dialog_;

  const extensions::Extension* InstallExtensionAndroid(
      const std::string& name) {
    static constexpr char kManifest[] =
        R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 3
         })";

    extensions::TestExtensionDir test_dir;
    test_dir.WriteManifest(kManifest);
    return LoadExtension(test_dir.UnpackedPath());
  }

 private:
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  base::test::ScopedFeatureList feature_list_;
};

MATCHER_P(IsExpectedMessage, extensions, "") {
  std::u16string expected_title;
  if (extensions.size() == 1) {
    std::u16string fixed_up_name =
        extensions::util::GetFixupExtensionNameForUIDisplay(
            base::UTF8ToUTF16(extensions[0]->name()));
    expected_title = l10n_util::GetStringFUTF16(
        IDS_EXTENSION_RELOAD_PAGE_BUBBLE_ALLOW_SINGLE_EXTENSION_TITLE,
        fixed_up_name);
  } else {
    expected_title = l10n_util::GetStringUTF16(
        IDS_EXTENSION_RELOAD_PAGE_BUBBLE_ALLOW_MULTIPLE_EXTENSIONS_TITLE);
  }

  if (arg->GetTitle() != expected_title) {
    *result_listener << "title is " << arg->GetTitle() << ", expected "
                     << expected_title;
    return false;
  }

  std::u16string expected_button_text =
      l10n_util::GetStringUTF16(IDS_EXTENSION_RELOAD_PAGE_BUBBLE_OK_BUTTON);
  if (arg->GetPrimaryButtonText() != expected_button_text) {
    *result_listener << "primary button text is " << arg->GetPrimaryButtonText()
                     << ", expected " << expected_button_text;
    return false;
  }

  if (extensions.size() > 1) {
    if (arg->GetIconBitmap().drawsNothing()) {
      *result_listener << "icon is empty, expected extension puzzle icon";
      return false;
    }
  }

  return true;
}

IN_PROC_BROWSER_TEST_F(ReloadPageDialogControllerAndroidBrowserTest,
                       ShowMessage_SingleExtension) {
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_NE(nullptr, web_contents);
  ASSERT_TRUE(NavigateToURL(web_contents, GURL("about:blank")));

  const extensions::Extension* extension = InstallExtensionAndroid("Extension");
  ASSERT_NE(nullptr, extension);
  std::vector<const extensions::Extension*> extensions = {extension};

  using testing::_;
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      message_dispatcher_bridge());
  EXPECT_CALL(*message_dispatcher_bridge(),
              EnqueueMessage(IsExpectedMessage(extensions), _,
                             messages::MessageScopeType::NAVIGATION,
                             messages::MessagePriority::kNormal))
      .Times(1);

  reload_page_dialog_ =
      std::make_unique<extensions::ReloadPageDialogController>(web_contents,
                                                               GetProfile());
  reload_page_dialog_->TriggerShow(extensions);
}

IN_PROC_BROWSER_TEST_F(ReloadPageDialogControllerAndroidBrowserTest,
                       ShowMessage_MultipleExtensions) {
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_NE(nullptr, web_contents);
  ASSERT_TRUE(NavigateToURL(web_contents, GURL("about:blank")));

  const extensions::Extension* extension1 =
      InstallExtensionAndroid("Extension1");
  ASSERT_NE(nullptr, extension1);
  const extensions::Extension* extension2 =
      InstallExtensionAndroid("Extension2");
  ASSERT_NE(nullptr, extension2);
  std::vector<const extensions::Extension*> extensions = {extension1,
                                                          extension2};

  using testing::_;
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      message_dispatcher_bridge());
  EXPECT_CALL(*message_dispatcher_bridge(),
              EnqueueMessage(IsExpectedMessage(extensions), _,
                             messages::MessageScopeType::NAVIGATION,
                             messages::MessagePriority::kNormal))
      .Times(1);

  reload_page_dialog_ =
      std::make_unique<extensions::ReloadPageDialogController>(web_contents,
                                                               GetProfile());
  reload_page_dialog_->TriggerShow(extensions);
}
