// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_VIEW_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_VIEW_TEST_BASE_H_

#include "chrome/browser/ui/views/controls/hover_button.h"
#include "content/public/browser/identity_request_account.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"

const std::u16string kRpETLDPlusOne = u"rp-example.com";
const std::u16string kIdpETLDPlusOne = u"idp-example.com";
const std::u16string kSecondIdpETLDPlusOne = u"idp2.com";
const std::u16string kTitleSignIn =
    u"Sign in to rp-example.com with idp-example.com";
const std::u16string kTitleSignInWithoutIdp = u"Sign in to rp-example.com";
const std::u16string kTitleSigningIn = u"Verifying…";
const std::u16string kTitleSigningInWithAutoReauthn = u"Signing you in…";
const std::u16string kTitleRequestPermission =
    u"Confirm you want to sign in to rp-example.com with "
    u"idp-example.com";
const std::u16string kBodySignIn = u"Choose an account to continue";

// The char version of `kIdpETLDPlusOne`.
inline constexpr char kIdpForDisplay[] = "idp-example.com";
// The char version of `kSecondIdpETLDPlusOne`.
inline constexpr char kSecondIdpForDisplay[] = "idp2.com";
inline constexpr char kIdBase[] = "id";
inline constexpr char kEmailBase[] = "email";
inline constexpr char kNameBase[] = "name";
inline constexpr char kGivenNameBase[] = "given_name";

inline constexpr char kTermsOfServiceUrl[] = "https://terms-of-service.com";
inline constexpr char kPrivacyPolicyUrl[] = "https://privacy-policy.com";
inline constexpr char kIdpBrandIconUrl[] = "https://idp-brand-icon.com";
inline constexpr char kRpBrandIconUrl[] = "https://rp-brand-icon.com";

extern const std::vector<content::IdentityRequestDialogDisclosureField>
    kDefaultDisclosureFields;

using IdentityProviderDataPtr = scoped_refptr<content::IdentityProviderData>;
using IdentityRequestAccountPtr =
    scoped_refptr<content::IdentityRequestAccount>;

// A base class for FedCM account selection view unit tests.
class AccountSelectionViewTestBase {
 public:
  AccountSelectionViewTestBase();
  AccountSelectionViewTestBase(const AccountSelectionViewTestBase&) = delete;
  AccountSelectionViewTestBase& operator=(const AccountSelectionViewTestBase&) =
      delete;
  ~AccountSelectionViewTestBase();

 protected:
  std::u16string GetHoverButtonTitle(HoverButton* account);
  views::Label* GetHoverButtonSubtitle(HoverButton* account);
  views::View* GetHoverButtonIconView(HoverButton* account);
  views::Label* GetHoverButtonFooter(HoverButton* account);
  views::View* GetHoverButtonSecondaryView(HoverButton* account);

  void CheckNonHoverableAccountRow(views::View* row,
                                   const std::string& account_suffix);
  void CheckHoverableAccountRows(
      const std::vector<raw_ptr<views::View, VectorExperimental>>& accounts,
      const std::vector<std::string>& account_suffixes,
      size_t& accounts_index,
      bool expect_idp = false,
      bool is_modal_dialog = false);
  void CheckDisclosureText(views::View* disclosure_text,
                           bool expect_terms_of_service,
                           bool expect_privacy_policy);

  IdentityRequestAccountPtr CreateTestIdentityRequestAccount(
      const std::string& account_suffix,
      IdentityProviderDataPtr idp,
      content::IdentityRequestAccount::LoginState login_state =
          content::IdentityRequestAccount::LoginState::kSignUp,
      std::optional<base::Time> last_used_timestamp = std::nullopt);
  // Creates a vector of accounts. When `login_states` are not passed, sets the
  // accounts' login states to LoginState::kSignUp. When `last_used_timestamps`
  // are not passed, sets accounts' last used timestamp to std::nullopt.
  std::vector<IdentityRequestAccountPtr> CreateTestIdentityRequestAccounts(
      const std::vector<std::string>& account_suffixes,
      IdentityProviderDataPtr idp,
      const std::vector<content::IdentityRequestAccount::LoginState>&
          login_states = {},
      const std::vector<std::optional<base::Time>>& last_used_timestamps = {});
  content::ClientMetadata CreateTestClientMetadata(
      const std::string& terms_of_service_url = kTermsOfServiceUrl,
      const std::string& privacy_policy_url = kPrivacyPolicyUrl,
      const std::string& rp_brand_icon_url = kRpBrandIconUrl);

  std::vector<std::string> GetChildClassNames(views::View* parent);
  views::View* GetViewWithClassName(views::View* parent,
                                    const std::string& class_name);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_VIEW_TEST_BASE_H_
