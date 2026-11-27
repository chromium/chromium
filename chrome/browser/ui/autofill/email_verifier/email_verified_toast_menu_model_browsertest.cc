// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"
#include "url/gurl.h"

namespace autofill {
namespace {

using EmailVerifiedToastMenuModelBrowserTest = InProcessBrowserTest;

// Verifies that showing the "Email verified" toast through
// ContentAutofillClient wires the "Manage" menu item through
// EmailVerificationController to open the contact info settings subpage.
IN_PROC_BROWSER_TEST_F(EmailVerifiedToastMenuModelBrowserTest,
                       ShowVerifiedToastManageItemNavigatesToSettings) {
  ContentAutofillClient* client = ContentAutofillClient::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_NE(client, nullptr);

  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  client->ShowEmailVerifiedToast(GURL("https://issuer.com"));

  ToastController* toast_controller = ToastController::From(browser());
  ASSERT_NE(toast_controller, nullptr);
  ASSERT_TRUE(toast_controller->IsShowingToast());
  EXPECT_EQ(toast_controller->GetCurrentToastId(), ToastId::kEmailVerified);

  toasts::ToastView* toast_view = toast_controller->GetToastViewForTesting();
  ASSERT_NE(toast_view, nullptr);
  ui::MenuModel* menu_model = toast_view->menu_model_for_testing();
  ASSERT_NE(menu_model, nullptr);
  ASSERT_EQ(menu_model->GetItemCount(), 1u);

  menu_model->ActivatedAt(0);

  EXPECT_EQ(
      user_action_tester.GetActionCount("Toast.EmailVerified.ManageClicked"),
      1);
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      chrome::GetSettingsUrl(chrome::kContactInfoSubPage));
  histogram_tester.ExpectTotalCount(
      "Autofill.YourSavedInfoSettingsPage.VisitReferrer", 0);
}

}  // namespace
}  // namespace autofill
