// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/smb_shares/smb_credentials_dialog.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace ash::smb_dialog {

const char kMountId[] = "mount-id";
const char kSharePath[] = "//test/share";

class SmbCredentialsDialogTest : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(SmbCredentialsDialogTest, CloseDialog) {
  content::WebContentsAddedObserver observer;

  base::RunLoop run_loop;
  SmbCredentialsDialog::Show(
      kMountId, kSharePath,
      base::BindLambdaForTesting([&run_loop](bool canceled,
                                             const std::string& username,
                                             const std::string& password) {
        EXPECT_TRUE(canceled);
        run_loop.Quit();
      }));

  content::WebContents* dialog_contents = observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(dialog_contents));
  EXPECT_EQ(dialog_contents->GetLastCommittedURL().host(),
            chrome::kChromeUISmbCredentialsHost);
  ASSERT_TRUE(content::ExecJs(dialog_contents, "chrome.send('dialogClose');"));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SmbCredentialsDialogTest, ShowSameMountId) {
  content::WebContentsAddedObserver observer;

  base::RunLoop run_loop;
  SmbCredentialsDialog::Show(
      kMountId, kSharePath,
      // This callback is replaced in the next call to Show() below, and
      // therefore will never be run.
      base::BindOnce([](bool canceled, const std::string& username,
                        const std::string& password) { FAIL(); }));
  SmbCredentialsDialog::Show(
      kMountId, kSharePath,
      base::BindLambdaForTesting([&run_loop](bool canceled,
                                             const std::string& username,
                                             const std::string& password) {
        EXPECT_TRUE(canceled);
        run_loop.Quit();
      }));

  content::WebContents* dialog_contents = observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(dialog_contents));
  EXPECT_EQ(dialog_contents->GetLastCommittedURL().host(),
            chrome::kChromeUISmbCredentialsHost);
  ASSERT_TRUE(content::ExecJs(dialog_contents, "chrome.send('dialogClose');"));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SmbCredentialsDialogTest, SubmitCredentials) {
  content::WebContentsAddedObserver observer;

  base::RunLoop run_loop;
  SmbCredentialsDialog::Show(
      kMountId, kSharePath,
      base::BindLambdaForTesting([&run_loop](bool canceled,
                                             const std::string& username,
                                             const std::string& password) {
        EXPECT_FALSE(canceled);
        EXPECT_EQ(username, "my-username");
        EXPECT_EQ(password, "my-password");
        run_loop.Quit();
      }));

  content::WebContents* dialog_contents = observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(dialog_contents));
  EXPECT_EQ(dialog_contents->GetLastCommittedURL().host(),
            chrome::kChromeUISmbCredentialsHost);
  ASSERT_TRUE(content::ExecJs(dialog_contents,
                              R"xxx(
const dialog = document.querySelector('smb-credentials-dialog');
dialog.username_ = 'my-username';
dialog.password_ = 'my-password';
dialog.$$('.action-button').click();
      )xxx"));

  run_loop.Run();
}

}  // namespace ash::smb_dialog
