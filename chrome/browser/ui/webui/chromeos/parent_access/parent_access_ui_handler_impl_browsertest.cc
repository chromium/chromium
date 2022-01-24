// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui_handler_impl.h"

#include "base/bind.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_browsertest_base.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

using ParentAccessUIHandlerImplBrowserTest =
    ParentAccessChildUserBrowserTestBase;

// Verify that the access token is successfully fetched.
IN_PROC_BROWSER_TEST_F(ParentAccessUIHandlerImplBrowserTest,
                       GetOAuthTokenSuccess) {
  // Open the Parent Access WebUI URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIParentAccessURL)));

  EXPECT_TRUE(content::WaitForLoadStop(contents()));

  ParentAccessUIHandlerImpl* handler = static_cast<ParentAccessUIHandlerImpl*>(
      GetParentAccessUI()->GetHandlerForTest());

  // Make sure the handler isn't null.
  ASSERT_NE(handler, nullptr);

  handler->GetOAuthToken(
      base::BindOnce([](parent_access_ui::mojom::GetOAuthTokenStatus status,
                        const std::string& token) -> void {
        EXPECT_EQ(parent_access_ui::mojom::GetOAuthTokenStatus::kSuccess,
                  status);
      }));
}

// Verifies that access token fetch errors are recorded.
IN_PROC_BROWSER_TEST_F(ParentAccessUIHandlerImplBrowserTest,
                       GetOAuthTokenError) {
  // Open the Parent Access WebUI URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIParentAccessURL)));

  EXPECT_TRUE(content::WaitForLoadStop(contents()));

  ParentAccessUIHandlerImpl* handler = static_cast<ParentAccessUIHandlerImpl*>(
      GetParentAccessUI()->GetHandlerForTest());

  // Make sure the handler isn't null.
  ASSERT_NE(handler, nullptr);

  // Trigger failure to issue access token.
  identity_test_env_->SetAutomaticIssueOfAccessTokens(false);

  handler->GetOAuthToken(
      base::BindOnce([](parent_access_ui::mojom::GetOAuthTokenStatus status,
                        const std::string& token) -> void {
        EXPECT_EQ(parent_access_ui::mojom::GetOAuthTokenStatus::kError, status);
      }));
}

// Verifies that only one access token fetch is possible at a time.
IN_PROC_BROWSER_TEST_F(ParentAccessUIHandlerImplBrowserTest,
                       GetOAuthTokenOnlyOneFetchAtATimeError) {
  // Open the Parent Access WebUI URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIParentAccessURL)));
  EXPECT_TRUE(content::WaitForLoadStop(contents()));

  ParentAccessUIHandlerImpl* handler = static_cast<ParentAccessUIHandlerImpl*>(
      GetParentAccessUI()->GetHandlerForTest());

  // Make sure the handler isn't null.
  ASSERT_NE(handler, nullptr);

  handler->GetOAuthToken(
      base::BindOnce([](parent_access_ui::mojom::GetOAuthTokenStatus status,
                        const std::string& token) -> void {
        EXPECT_EQ(parent_access_ui::mojom::GetOAuthTokenStatus::kSuccess,
                  status);
      }));

  handler->GetOAuthToken(
      base::BindOnce([](parent_access_ui::mojom::GetOAuthTokenStatus status,
                        const std::string& token) -> void {
        EXPECT_EQ(
            parent_access_ui::mojom::GetOAuthTokenStatus::kOnlyOneFetchAtATime,
            status);
      }));
}

}  // namespace chromeos
