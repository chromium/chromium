// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/login_screen_extension_ui/dialog_delegate.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/extensions/login_screen_ui/ui_handler.h"
#include "chrome/browser/ui/ash/login/login_screen_extension_ui/create_options.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "url/url_constants.h"

namespace ash {
namespace login_screen_extension_ui {
namespace {

using DialogDelegateUnittest = ::testing::Test;

const char kExtensionName[] = "extension-name";
const char16_t kExtensionName16[] = u"extension-name";
const char kExtensionId[] = "abcdefghijklmnopqrstuvwxyzabcdef";
const char kResourcePath[] = "path/to/file.html";

}  // namespace

TEST_F(DialogDelegateUnittest, Test) {
  content::BrowserTaskEnvironment task_environment_;

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(kExtensionName).SetID(kExtensionId).Build();

  base::RunLoop close_callback_wait;

  CreateOptions create_options(
      extension->short_name(), extension->GetResourceURL(kResourcePath),
      false /*can_be_closed_by_user*/, close_callback_wait.QuitClosure());

  // `delegate` will delete itself when calling `OnDialogClosed()` at the end of
  // the test.
  DialogDelegate* delegate = new DialogDelegate(&create_options);

  EXPECT_EQ(ui::mojom::ModalType::kWindow, delegate->GetDialogModalType());
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_LOGIN_EXTENSION_UI_DIALOG_TITLE,
                                       kExtensionName16),
            delegate->GetDialogTitle());
  EXPECT_EQ(extensions::Extension::GetResourceURL(
                extensions::Extension::GetBaseURLFromExtensionId(kExtensionId),
                kResourcePath),
            delegate->GetDialogContentURL());
  EXPECT_FALSE(delegate->can_resize());
  EXPECT_TRUE(delegate->ShouldShowDialogTitle());
  EXPECT_TRUE(delegate->ShouldCenterDialogTitleText());
  EXPECT_FALSE(delegate->ShouldCloseDialogOnEscape());

  EXPECT_FALSE(delegate->OnDialogCloseRequested());
  delegate->set_can_close(true);
  EXPECT_TRUE(delegate->OnDialogCloseRequested());

  delegate->OnDialogClosed(std::string());
  close_callback_wait.Run();
}

}  // namespace login_screen_extension_ui
}  // namespace ash
