// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/simple_alert_infobar_delegate.h"

#include <memory>

#include "third_party/skia/include/core/SkBitmap.h"

SimpleAlertInfoBarDelegate::SimpleAlertInfoBarDelegate(
    infobars::InfoBarDelegate::InfoBarIdentifier infobar_identifier,
    const gfx::VectorIcon* vector_icon,
    const std::u16string& message,
    bool auto_expire,
    bool should_animate,
    bool closeable)
    : infobar_identifier_(infobar_identifier),
      vector_icon_(vector_icon),
      message_(message),
      auto_expire_(auto_expire),
      should_animate_(should_animate),
      closeable_(closeable) {}

SimpleAlertInfoBarDelegate::~SimpleAlertInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
SimpleAlertInfoBarDelegate::GetIdentifier() const {
  return infobar_identifier_;
}

const gfx::VectorIcon& SimpleAlertInfoBarDelegate::GetVectorIcon() const {
  return vector_icon_ ? *vector_icon_ : InfoBarDelegate::GetVectorIcon();
}

bool SimpleAlertInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return auto_expire_ && ConfirmInfoBarDelegate::ShouldExpire(details);
}

bool SimpleAlertInfoBarDelegate::ShouldAnimate() const {
  return should_animate_ && ConfirmInfoBarDelegate::ShouldAnimate();
}

std::u16string SimpleAlertInfoBarDelegate::GetMessageText() const {
  return message_;
}

int SimpleAlertInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

bool SimpleAlertInfoBarDelegate::IsCloseable() const {
  return closeable_;
}
