// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_view_test_base.h"

#include "chrome/browser/ui/views/webid/account_selection_view_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"

const std::vector<content::IdentityRequestDialogDisclosureField>
    kDefaultDisclosureFields = {
        content::IdentityRequestDialogDisclosureField::kName,
        content::IdentityRequestDialogDisclosureField::kEmail,
        content::IdentityRequestDialogDisclosureField::kPicture};

AccountSelectionViewTestBase::AccountSelectionViewTestBase() = default;

AccountSelectionViewTestBase::~AccountSelectionViewTestBase() = default;

std::u16string AccountSelectionViewTestBase::GetHoverButtonTitle(
    HoverButton* account) {
  return account->title()->GetText();
}

views::Label* AccountSelectionViewTestBase::GetHoverButtonSubtitle(
    HoverButton* account) {
  return account->subtitle();
}

views::View* AccountSelectionViewTestBase::GetHoverButtonIconView(
    HoverButton* account) {
  return account->icon_view();
}

views::Label* AccountSelectionViewTestBase::GetHoverButtonFooter(
    HoverButton* account) {
  return account->footer();
}

views::View* AccountSelectionViewTestBase::GetHoverButtonSecondaryView(
    HoverButton* account) {
  return account->secondary_view();
}

IdentityRequestAccountPtr
AccountSelectionViewTestBase::CreateTestIdentityRequestAccount(
    const std::string& account_suffix,
    IdentityProviderDataPtr idp,
    content::IdentityRequestAccount::LoginState login_state,
    std::optional<base::Time> last_used_timestamp) {
  IdentityRequestAccountPtr account =
      base::MakeRefCounted<content::IdentityRequestAccount>(
          std::string(kIdBase) + account_suffix,
          std::string(kEmailBase) + account_suffix,
          std::string(kNameBase) + account_suffix,
          std::string(kGivenNameBase) + account_suffix, GURL(),
          /*login_hints=*/std::vector<std::string>(),
          /*domain_hints=*/std::vector<std::string>(),
          /*labels=*/std::vector<std::string>(), login_state,
          /*browser_trusted_login_state=*/
          content::IdentityRequestAccount::LoginState::kSignUp,
          last_used_timestamp);
  account->identity_provider = std::move(idp);
  return account;
}

std::vector<IdentityRequestAccountPtr>
AccountSelectionViewTestBase::CreateTestIdentityRequestAccounts(
    const std::vector<std::string>& account_suffixes,
    IdentityProviderDataPtr idp,
    const std::vector<content::IdentityRequestAccount::LoginState>&
        login_states,
    const std::vector<std::optional<base::Time>>& last_used_timestamps) {
  if (!login_states.empty()) {
    CHECK_EQ(account_suffixes.size(), login_states.size());
  }
  if (!last_used_timestamps.empty()) {
    CHECK_EQ(account_suffixes.size(), last_used_timestamps.size());
  }
  std::vector<IdentityRequestAccountPtr> accounts;
  size_t idx = 0;
  for (const std::string& account_suffix : account_suffixes) {
    content::IdentityRequestAccount::LoginState login_state =
        login_states.empty()
            ? content::IdentityRequestAccount::LoginState::kSignUp
            : login_states[idx];
    std::optional<base::Time> last_used_timestamp =
        last_used_timestamps.empty() ? std::nullopt : last_used_timestamps[idx];
    accounts.push_back(CreateTestIdentityRequestAccount(
        account_suffix, idp, login_state, last_used_timestamp));
    ++idx;
  }
  return accounts;
}

content::ClientMetadata AccountSelectionViewTestBase::CreateTestClientMetadata(
    const std::string& terms_of_service_url,
    const std::string& privacy_policy_url,
    const std::string& rp_brand_icon_url) {
  return content::ClientMetadata(GURL(terms_of_service_url),
                                 GURL(privacy_policy_url),
                                 GURL(rp_brand_icon_url));
}

std::vector<std::string> AccountSelectionViewTestBase::GetChildClassNames(
    views::View* parent) {
  std::vector<std::string> child_class_names;
  for (views::View* child_view : parent->children()) {
    child_class_names.push_back(child_view->GetClassName());
  }
  return child_class_names;
}

views::View* AccountSelectionViewTestBase::GetViewWithClassName(
    views::View* parent,
    const std::string& class_name) {
  for (views::View* child_view : parent->children()) {
    if (child_view->GetClassName() == class_name) {
      return child_view;
    }
  }
  return nullptr;
}

void AccountSelectionViewTestBase::CheckNonHoverableAccountRow(
    views::View* row,
    const std::string& account_suffix) {
  std::vector<raw_ptr<views::View, VectorExperimental>> row_children =
      row->children();
  ASSERT_EQ(row_children.size(), 2u);

  // Check the image.
  views::ImageView* image_view =
      static_cast<views::ImageView*>(row_children[0]);
  EXPECT_TRUE(image_view);

  // Check the text shown.
  views::View* text_view = row_children[1];
  views::BoxLayout* layout_manager =
      static_cast<views::BoxLayout*>(text_view->GetLayoutManager());
  ASSERT_TRUE(layout_manager);
  EXPECT_EQ(layout_manager->GetOrientation(),
            views::BoxLayout::Orientation::kVertical);
  std::vector<raw_ptr<views::View, VectorExperimental>> text_view_children =
      text_view->children();
  ASSERT_EQ(text_view_children.size(), 2u);

  std::string expected_name(std::string(kNameBase) + account_suffix);
  views::StyledLabel* name_view =
      static_cast<views::StyledLabel*>(text_view_children[0]);
  ASSERT_TRUE(name_view);
  EXPECT_EQ(name_view->GetText(), base::UTF8ToUTF16(expected_name));

  std::string expected_email(std::string(kEmailBase) + account_suffix);
  views::Label* email_view = static_cast<views::Label*>(text_view_children[1]);
  ASSERT_TRUE(email_view);
  EXPECT_EQ(email_view->GetText(), base::UTF8ToUTF16(expected_email));
}

void AccountSelectionViewTestBase::CheckHoverableAccountRows(
    const std::vector<raw_ptr<views::View, VectorExperimental>>& accounts,
    const std::vector<std::string>& account_suffixes,
    size_t& accounts_index,
    bool expect_idp,
    bool is_modal_dialog) {
  ASSERT_GE(accounts.size(), account_suffixes.size() + accounts_index);
  // Checks the account rows starting at `accounts[accounts_index]`. Updates
  // `accounts_index` to the first unused index in `accounts`, or to
  // `accounts.size()` if done.
  for (const auto& account_suffix : account_suffixes) {
    if (std::string(accounts[accounts_index]->GetClassName()) == "Separator") {
      ++accounts_index;
    }
    ASSERT_STREQ("HoverButton", accounts[accounts_index]->GetClassName());
    HoverButton* account_row =
        static_cast<HoverButton*>(accounts[accounts_index++]);
    ASSERT_TRUE(account_row);

    // Check for account name in title.
    EXPECT_EQ(GetHoverButtonTitle(account_row),
              base::UTF8ToUTF16(kNameBase + account_suffix));

    // Check for account email in subtitle.
    EXPECT_EQ(GetHoverButtonSubtitle(account_row)->GetText(),
              base::UTF8ToUTF16(std::string(kEmailBase) + account_suffix));
    // The subtitle has changed style, so AutoColorReadabilityEnabled should be
    // set.
    EXPECT_TRUE(
        GetHoverButtonSubtitle(account_row)->GetAutoColorReadabilityEnabled());

    // Check for account icon.
    views::View* icon_view = GetHoverButtonIconView(account_row);
    EXPECT_TRUE(icon_view);

    // Check for the IDP eTLD+1 in footer. This is not passed to the method but
    // in our tests they all start with 'idp'.
    if (expect_idp) {
      EXPECT_TRUE(
          GetHoverButtonFooter(account_row)->GetText().starts_with(u"idp"));
    } else {
      EXPECT_FALSE(GetHoverButtonFooter(account_row));
    }
    EXPECT_EQ(icon_view->size(),
              is_modal_dialog ? gfx::Size(kModalAvatarSize, kModalAvatarSize)
              : expect_idp    ? gfx::Size(kDesiredAvatarSize + kIdpBadgeOffset,
                                          kDesiredAvatarSize + kIdpBadgeOffset)
                           : gfx::Size(kDesiredAvatarSize, kDesiredAvatarSize));

    // Check for arrow icon in secondary view.
    if (is_modal_dialog) {
      views::ImageView* arrow_icon_view = static_cast<views::ImageView*>(
          GetHoverButtonSecondaryView(account_row));
      EXPECT_TRUE(arrow_icon_view);
    } else {
      EXPECT_FALSE(GetHoverButtonSecondaryView(account_row));
    }
    if (expect_idp) {
      std::vector<raw_ptr<views::View, VectorExperimental>> icon_children =
          icon_view->children();
      ASSERT_EQ(icon_children.size(), 2u);
      EXPECT_STREQ(icon_children[0]->GetClassName(), "AccountImageView");
      EXPECT_EQ(icon_children[0]->size(),
                gfx::Size(kDesiredAvatarSize + kIdpBadgeOffset,
                          kDesiredAvatarSize + kIdpBadgeOffset));
      EXPECT_STREQ(icon_children[1]->GetClassName(), "BoxLayoutView");
      ASSERT_EQ(icon_children[1]->children().size(), 1u);
      views::View* brand_icon_image_view = icon_children[1]->children()[0];
      EXPECT_STREQ(brand_icon_image_view->GetClassName(), "BrandIconImageView");
    } else {
      EXPECT_STREQ(icon_view->GetClassName(), "AccountImageView");
    }
  }
}

void AccountSelectionViewTestBase::CheckDisclosureText(
    views::View* disclosure_text,
    bool expect_terms_of_service,
    bool expect_privacy_policy) {
  views::StyledLabel* disclosure_label =
      static_cast<views::StyledLabel*>(disclosure_text);
  ASSERT_TRUE(disclosure_label);

  std::u16string expected_disclosure_text =
      u"To continue, idp-example.com will share your name, email address, and "
      u"profile picture with this site.";
  if (expect_privacy_policy && expect_terms_of_service) {
    expected_disclosure_text +=
        u" See this site's privacy policy and terms of service.";
  } else if (expect_privacy_policy) {
    expected_disclosure_text += u" See this site's privacy policy.";
  } else if (expect_terms_of_service) {
    expected_disclosure_text += u" See this site's terms of service.";
  }

  EXPECT_EQ(disclosure_label->GetText(), expected_disclosure_text);
}
