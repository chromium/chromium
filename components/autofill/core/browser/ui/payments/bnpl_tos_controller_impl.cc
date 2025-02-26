// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"  // TODO: crbug.com/391141123 - Remove when the controller is implemented.
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_view.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using l10n_util::GetStringFUTF16;
using l10n_util::GetStringUTF16;
using std::u16string;

namespace autofill {

namespace {
constexpr std::string_view kWalletLinkText = "wallet.google.com";
constexpr std::string_view kWalletUrlString = "https://wallet.google.com/";
}  // namespace

BnplTosControllerImpl::BnplTosControllerImpl() = default;

BnplTosControllerImpl::~BnplTosControllerImpl() = default;

void BnplTosControllerImpl::OnViewClosing(bool user_accepted) {
  // The view is being closed so set the pointer to nullptr.
  view_.reset();
}

u16string BnplTosControllerImpl::GetOkButtonLabel() const {
  return GetStringUTF16(IDS_AUTOFILL_BNPL_TOS_OK_BUTTON_LABEL);
}

u16string BnplTosControllerImpl::GetCancelButtonLabel() const {
  return GetStringUTF16(IDS_AUTOFILL_BNPL_TOS_CANCEL_BUTTON_LABEL);
}

u16string BnplTosControllerImpl::GetTitle() const {
  return GetStringFUTF16(IDS_AUTOFILL_BNPL_TOS_TITLE, issuer_name_);
}

u16string BnplTosControllerImpl::GetReviewText() const {
  return GetStringFUTF16(IDS_AUTOFILL_BNPL_TOS_REVIEW_TEXT, issuer_name_);
}

u16string BnplTosControllerImpl::GetApproveText() const {
  return GetStringFUTF16(IDS_AUTOFILL_BNPL_TOS_APPROVE_TEXT, issuer_name_);
}

TextWithLink BnplTosControllerImpl::GetLinkText() const {
  TextWithLink text_with_link;
  std::vector<size_t> offsets;
  text_with_link.text =
      GetStringFUTF16(IDS_AUTOFILL_BNPL_TOS_LINK_TEXT, issuer_name_,
                      base::UTF8ToUTF16(kWalletLinkText), &offsets);

  // The link is the second replacement string making it the second offset.
  text_with_link.offset =
      gfx::Range(offsets[1], offsets[1] + kWalletLinkText.length());

  text_with_link.url = GURL(kWalletUrlString);

  return text_with_link;
}

const LegalMessageLines& BnplTosControllerImpl::GetLegalMessageLines() const {
  return legal_message_lines_;
}

AccountInfo BnplTosControllerImpl::GetAccountInfo() const {
  // TODO: crbug.com/391141123 - Actually get the account info when the
  // controller is implemented.
  AccountInfo account_info = AccountInfo();
  account_info.email =
      "somebody@example.test";  // Temporary email to verify the view.
  return account_info;
}

base::WeakPtr<BnplTosController> BnplTosControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BnplTosControllerImpl::Show(
    base::OnceCallback<std::unique_ptr<BnplTosView>()>
        create_and_show_view_callback) {
  // If the view already exists, don't create and show a new view.
  if (view_) {
    return;
  }

  // TODO: crbug.com/391141123 - Pass in the issuer name and legal lines from
  // the controller when it is implemented.
  issuer_name_ = u"Affirm";
  std::string legal_lines_as_json_string =
      "{ \"line\" : [ { \"template\": \"By continuing, you agree to the {0} "
      "and that Google Pay may share or receive some data from Affirm, such as "
      "transaction or account data, in order to provide this service. The {1} "
      "describes how Google Pay handles your data. Eligibility and payment "
      "plans are provided by Affirm, who processes your data in accordance "
      "with their {2}.\", \"template_parameter\": [ { \"display_text\": "
      "\"Google Pay Terms of Service\", \"url\": \"http://www.example.com/\" "
      "}, { \"display_text\": "
      "\"Google Pay Privacy Notice\", \"url\": \"http://www.example.com/\" }, "
      "{ \"display_text\": "
      "\"privacy notice\", \"url\": \"http://www.example.com/\" } "
      "] }] }";
  std::optional<base::Value> legal_lines_as_json(
      (base::JSONReader::Read(legal_lines_as_json_string)));
  LegalMessageLine::Parse(legal_lines_as_json->GetDict(), &legal_message_lines_,
                          true);

  view_ = std::move(create_and_show_view_callback).Run();
}

}  // namespace autofill
