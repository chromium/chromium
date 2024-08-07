// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_test_util.h"
#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/fake_server_network_resources.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/test_event.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/widget_test.h"

namespace {

using password_manager::metrics_util::PasswordType;

void PerformMouseClickOnView(views::View* view) {
  ui::AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;
  view->HandleAccessibleAction(data);
}

// Clicks the location icon to open the page info bubble.
void OpenPageInfoBubble(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  LocationIconView* location_icon_view =
      browser_view->toolbar()->location_bar()->location_icon_view();
  ASSERT_TRUE(location_icon_view);
  ui::test::TestEvent event;
  location_icon_view->ShowBubble(event);
  views::BubbleDialogDelegateView* page_info =
      PageInfoBubbleView::GetPageInfoBubbleForTesting();
  EXPECT_NE(nullptr, page_info);
  page_info->set_close_on_deactivate(false);
}

// Opens the Page Info bubble and retrieves the UI view identified by
// |view_id|.
views::View* GetView(Browser* browser, int view_id) {
  views::Widget* page_info_bubble =
      PageInfoBubbleView::GetPageInfoBubbleForTesting()->GetWidget();
  EXPECT_TRUE(page_info_bubble);

  views::View* view = page_info_bubble->GetRootView()->GetViewByID(view_id);
  EXPECT_TRUE(view);
  return view;
}

}  // namespace

// This test suite tests functionality that requires Sync to be active.
class PageInfoBubbleViewSyncBrowserTest : public SyncTest {
 public:
  PageInfoBubbleViewSyncBrowserTest() : SyncTest(SINGLE_CLIENT) {}

  PageInfoBubbleViewSyncBrowserTest(
      const PageInfoBubbleViewSyncBrowserTest& chip) = delete;
  PageInfoBubbleViewSyncBrowserTest& operator=(
      const PageInfoBubbleViewSyncBrowserTest& chip) = delete;

 protected:
  void SetupSyncForAccount() {
    ASSERT_TRUE(SetupClients());

    // Sign the profile in.
    ASSERT_TRUE(
        GetClient(0)->SignInPrimaryAccount(signin::ConsentLevel::kSync));

    CoreAccountInfo current_info =
        IdentityManagerFactory::GetForProfile(GetProfile(0))
            ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
    // Need to update hosted domain since it is not populated.
    AccountInfo account_info;
    account_info.account_id = current_info.account_id;
    account_info.gaia = current_info.gaia;
    account_info.email = current_info.email;
    account_info.hosted_domain = kNoHostedDomainFound;
    signin::UpdateAccountInfoForAccount(
        IdentityManagerFactory::GetForProfile(GetProfile(0)), account_info);

    ASSERT_TRUE(SetupSync());
  }

  const std::u16string GetPageInfoBubbleViewDetailText() {
    auto* label =
        PageInfoBubbleView::GetPageInfoBubbleForTesting()->GetViewByID(
            PageInfoViewFactory::VIEW_ID_PAGE_INFO_SECURITY_DETAILS_LABEL);
    return static_cast<views::StyledLabel*>(label)->GetText();
  }
};

// Test opening page info bubble that matches
// SB_THREAT_TYPE_GAIA_PASSWORD_REUSE threat type.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewSyncBrowserTest,
                       VerifySignInPasswordReusePageInfoBubble) {
  // PageInfo calls GetPasswordProtectionReusedPasswordAccountType which checks
  // to see if the account is syncing.
  SetupSyncForAccount();

  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(safe_browsing::kSyncPasswordPageInfoHistogram, 0);

  AddBlankTabAndShow(GetBrowser(0));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      GetBrowser(0), embedded_test_server()->GetURL("/")));
  // Update security state of the current page to match
  // SB_THREAT_TYPE_GAIA_PASSWORD_REUSE.
  safe_browsing::ChromePasswordProtectionService* service =
      safe_browsing::ChromePasswordProtectionService::
          GetPasswordProtectionService(GetProfile(0));
  service->set_username_for_last_shown_warning("user@gmail.com");
  content::WebContents* contents =
      GetBrowser(0)->tab_strip_model()->GetActiveWebContents();
  safe_browsing::ReusedPasswordAccountType account_type;
  account_type.set_account_type(
      safe_browsing::ReusedPasswordAccountType::GMAIL);
  account_type.set_is_account_syncing(true);
  service->set_reused_password_account_type_for_last_shown_warning(
      account_type);
  scoped_refptr<safe_browsing::PasswordProtectionRequest> request =
      safe_browsing::CreateDummyRequest(contents);
  service->ShowModalWarning(
      request.get(),
      safe_browsing::LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      "unused_token", account_type);

  OpenPageInfoBubble(GetBrowser(0));
  views::View* change_password_button =
      GetView(GetBrowser(0),
              PageInfoViewFactory::VIEW_ID_PAGE_INFO_BUTTON_CHANGE_PASSWORD);
  views::View* safelist_password_reuse_button = GetView(
      GetBrowser(0),
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_BUTTON_ALLOWLIST_PASSWORD_REUSE);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();
  ASSERT_EQ(
      security_state::MALICIOUS_CONTENT_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE,
      visible_security_state->malicious_content_status);
  ASSERT_EQ(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SYNC) +
          u" " + l10n_util::GetStringUTF16(IDS_LEARN_MORE),
      GetPageInfoBubbleViewDetailText());

  // Verify these two buttons are showing.
  EXPECT_TRUE(change_password_button->GetVisible());
  EXPECT_TRUE(safelist_password_reuse_button->GetVisible());

  // Verify clicking on button will increment corresponding bucket of
  // PasswordProtection.PageInfoAction.SyncPasswordEntry histogram.
  PerformMouseClickOnView(change_password_button);
  EXPECT_THAT(
      histograms.GetAllSamples(safe_browsing::kSyncPasswordPageInfoHistogram),
      testing::ElementsAre(
          base::Bucket(static_cast<int>(safe_browsing::WarningAction::SHOWN),
                       1),
          base::Bucket(
              static_cast<int>(safe_browsing::WarningAction::CHANGE_PASSWORD),
              1)));

  PerformMouseClickOnView(safelist_password_reuse_button);
  EXPECT_THAT(
      histograms.GetAllSamples(safe_browsing::kSyncPasswordPageInfoHistogram),
      testing::ElementsAre(
          base::Bucket(static_cast<int>(safe_browsing::WarningAction::SHOWN),
                       1),
          base::Bucket(
              static_cast<int>(safe_browsing::WarningAction::CHANGE_PASSWORD),
              1),
          base::Bucket(static_cast<int>(
                           safe_browsing::WarningAction::MARK_AS_LEGITIMATE),
                       1)));
  // Security state will change after allowlisting.
  visible_security_state = helper->GetVisibleSecurityState();
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            visible_security_state->malicious_content_status);
}
