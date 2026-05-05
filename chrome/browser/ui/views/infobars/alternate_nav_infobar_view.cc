// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/alternate_nav_infobar_view.h"

#include <stddef.h>

#include <utility>

#include "base/check_op.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"

// AlternateNavInfoBarDelegate -------------------------------------------------

// static
std::unique_ptr<infobars::InfoBar> AlternateNavInfoBarDelegate::CreateInfoBar(
    std::unique_ptr<AlternateNavInfoBarDelegate> delegate) {
  return std::make_unique<AlternateNavInfoBarView>(std::move(delegate));
}

// AlternateNavInfoBarView -----------------------------------------------------

AlternateNavInfoBarView::AlternateNavInfoBarView(
    std::unique_ptr<AlternateNavInfoBarDelegate> delegate)
    : InfoBarView(std::move(delegate)) {
  auto* delegate_ptr = GetDelegate();
  size_t offset;
  std::u16string message_text = delegate_ptr->GetMessageTextWithOffset(&offset);
  DCHECK_NE(std::u16string::npos, offset);
  label_1_text_ = message_text.substr(0, offset);

  label_1_ = content_container()->AddChildView(CreateLabel(label_1_text_));

  link_text_ = delegate_ptr->GetLinkText();
  link_ = content_container()->AddChildView(CreateLink(link_text_));

  label_2_text_ = message_text.substr(offset);
  label_2_ = content_container()->AddChildView(CreateLabel(label_2_text_));
}

AlternateNavInfoBarView::~AlternateNavInfoBarView() = default;

// static

void AlternateNavInfoBarView::Layout(PassKey) {
  LayoutSuperclass<InfoBarView>(this);
}

int AlternateNavInfoBarView::GetContentMinimumWidth() const {
  return 0;
}

AlternateNavInfoBarDelegate* AlternateNavInfoBarView::GetDelegate() {
  return static_cast<AlternateNavInfoBarDelegate*>(delegate());
}
