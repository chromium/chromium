// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/sms/sms_infobar_delegate.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

SmsInfoBarDelegate::SmsInfoBarDelegate(const url::Origin& origin,
                                       const std::string& one_time_code,
                                       base::OnceClosure on_confirm,
                                       base::OnceClosure on_cancel)
    : ConfirmInfoBarDelegate(),
      origin_(origin),
      one_time_code_(one_time_code),
      on_confirm_(std::move(on_confirm)),
      on_cancel_(std::move(on_cancel)) {}

SmsInfoBarDelegate::~SmsInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier SmsInfoBarDelegate::GetIdentifier()
    const {
  return SMS_RECEIVER_INFOBAR_DELEGATE;
}

int SmsInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_PHONE_ICON;
}

base::string16 SmsInfoBarDelegate::GetMessageText() const {
  base::string16 origin = url_formatter::FormatOriginForSecurityDisplay(
      origin_, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  return l10n_util::GetStringFUTF16(IDS_SMS_INFOBAR_STATUS_SMS_RECEIVED,
                                    base::UTF8ToUTF16(one_time_code_), origin);
}

int SmsInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 SmsInfoBarDelegate::GetButtonLabel(InfoBarButton button) const {
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

base::string16 SmsInfoBarDelegate::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_SMS_INFOBAR_TITLE);
}
