// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/sms/android/sms_infobar_delegate.h"

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar.h"
#include "components/resources/android/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace sms {

SmsInfoBarDelegate::SmsInfoBarDelegate(const OriginList& origin_list,
                                       const std::string& one_time_code,
                                       base::OnceClosure on_confirm,
                                       base::OnceClosure on_cancel)
    : ConfirmInfoBarDelegate(),
      origin_list_(origin_list),
      one_time_code_(one_time_code),
      on_confirm_(std::move(on_confirm)),
      on_cancel_(std::move(on_cancel)) {}

SmsInfoBarDelegate::~SmsInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier SmsInfoBarDelegate::GetIdentifier()
    const {
  return WEBOTP_SERVICE_INFOBAR_DELEGATE;
}

int SmsInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_PHONE_ICON;
}

std::u16string SmsInfoBarDelegate::GetMessageText() const {
  if (origin_list_.size() == 1) {
    std::u16string origin = url_formatter::FormatOriginForSecurityDisplay(
        origin_list_[0], url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    return l10n_util::GetStringFUTF16(IDS_SMS_INFOBAR_STATUS_SMS_RECEIVED,
                                      base::UTF8ToUTF16(one_time_code_),
                                      origin);
  }

  // Only one cross-origin iframe is allowed.
  DCHECK_EQ(origin_list_.size(), 2u);

  std::u16string embedded_origin =
      url_formatter::FormatOriginForSecurityDisplay(
          origin_list_[0], url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  std::u16string top_origin = url_formatter::FormatOriginForSecurityDisplay(
      origin_list_[1], url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  return l10n_util::GetStringFUTF16(
      IDS_SMS_INFOBAR_STATUS_SMS_RECEIVED_FROM_EMBEDDED_FRAME,
      base::UTF8ToUTF16(one_time_code_), top_origin, embedded_origin);
}

int SmsInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string SmsInfoBarDelegate::GetButtonLabel(InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_SMS_INFOBAR_BUTTON_OK);
}

bool SmsInfoBarDelegate::Accept() {
  std::move(on_confirm_).Run();
  return true;
}

void SmsInfoBarDelegate::InfoBarDismissed() {
  std::move(on_cancel_).Run();
}

std::u16string SmsInfoBarDelegate::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_SMS_INFOBAR_TITLE);
}

}  // namespace sms
