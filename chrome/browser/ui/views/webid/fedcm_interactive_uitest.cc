// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class runs CUJ tests for fedCM. Normally fedCM is invoked by javascript
// from webpages, and requires communication with multiple remote endpoints.
// This test suite does not do any of that at the moment.

#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/webid/account_selection_view_test_base.h"
#include "chrome/browser/ui/views/webid/fake_delegate.h"
#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/interactive_test.h"

namespace webid {
namespace {

class FedCmCUJTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  auto OpenAccounts(blink::mojom::RpMode mode) {
    return Do([this, mode]() {
      delegate_ = std::make_unique<FakeDelegate>(
          browser()->GetActiveTabInterface()->GetContents());
      account_selection_view_ = std::make_unique<FedCmAccountSelectionView>(
          delegate_.get(), browser()->GetActiveTabInterface());
      idps_ = {base::MakeRefCounted<content::IdentityProviderData>(
          "idp-example.com", content::IdentityProviderMetadata(),
          content::ClientMetadata(GURL(), GURL(), GURL(), gfx::Image()),
          blink::mojom::RpContext::kSignIn, /*format=*/std::nullopt,
          kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/false)};
      accounts_ = {base::MakeRefCounted<Account>(
          "id", "display_identifier", "display_name", "email", "name",
          "given_name", GURL(), "phone", "username",
          /*login_hints=*/std::vector<std::string>(),
          /*domain_hints=*/std::vector<std::string>(),
          /*labels=*/std::vector<std::string>())};
      accounts_[0]->identity_provider = idps_[0];
      account_selection_view_->Show(
          content::RelyingPartyData(u"rp-example.com",
                                    /*iframe_for_display=*/u""),
          idps_, accounts_, mode,
          /*new_accounts=*/std::vector<IdentityRequestAccountPtr>());
    });
  }

  // Opens the modal version of the account chooser.
  auto OpenAccountsModal() {
    return OpenAccounts(blink::mojom::RpMode::kActive);
  }

  // Opens the bubble version of the account chooser.
  auto OpenAccountsBubble() {
    return OpenAccounts(blink::mojom::RpMode::kPassive);
  }

  auto ShowTabModalUI() {
    return Do([this]() {
      tab_modal_ui_ = browser()->GetActiveTabInterface()->ShowModalUI();
    });
  }
  auto HideTabModalUI() {
    return Do([this]() { tab_modal_ui_.reset(); });
  }

 protected:
  std::unique_ptr<FakeDelegate> delegate_;
  std::vector<IdentityProviderDataPtr> idps_;
  std::vector<IdentityRequestAccountPtr> accounts_;
  std::unique_ptr<FedCmAccountSelectionView> account_selection_view_;
  std::unique_ptr<tabs::ScopedTabModalUI> tab_modal_ui_;
};

// Shows the account picker. Selects an account.
IN_PROC_BROWSER_TEST_F(FedCmCUJTest, SelectAccount) {
  RunTestSequence(OpenAccountsModal(),
                  WaitForShow(kFedCmAccountChooserDialogAccountElementId),
                  PressButton(kFedCmAccountChooserDialogAccountElementId));
}

// Shows the bubble account picker. It should hide when a modal UI is shown. It
// should re-show when the modal UI goes away.
IN_PROC_BROWSER_TEST_F(FedCmCUJTest, BubbleHidesWhenModalUIShown) {
  RunTestSequence(
      OpenAccountsBubble(),
      WaitForShow(kFedCmAccountChooserDialogAccountElementId), ShowTabModalUI(),
      WaitForHide(kFedCmAccountChooserDialogAccountElementId), HideTabModalUI(),
      WaitForShow(kFedCmAccountChooserDialogAccountElementId));
}

// TODO(https://crbug.com/387473078): Fix this on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_OneClickOutsideBubble DISABLED_OneClickOutsideBubble
#else
#define MAYBE_OneClickOutsideBubble OneClickOutsideBubble
#endif
// When the bubble view is showing, a single click outside the bubble should be
// received by the website.
IN_PROC_BROWSER_TEST_F(FedCmCUJTest, MAYBE_OneClickOutsideBubble) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  // chrome/test/data/button.html
  const GURL url = embedded_test_server()->GetURL("/button.html");
  const DeepQuery kPathToButton{
      "button",
  };

  RunTestSequence(
      InstrumentTab(kActiveTab), NavigateWebContents(kActiveTab, url),
      OpenAccountsBubble(),
      WaitForShow(kFedCmAccountChooserDialogAccountElementId),
      MoveMouseTo(kActiveTab, kPathToButton),
      CheckJsResult(
          kActiveTab,
          "() => document.getElementById('text').style.display == 'none'"),
      ClickMouse(),
      CheckJsResult(
          kActiveTab,
          "() => document.getElementById('text').style.display == 'block'"));
}

}  // namespace
}  // namespace webid
