// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/page_info_infobar_delegate.h"

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

// static
void PageInfoInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager) {
  infobar_manager->AddInfoBar(CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(new PageInfoInfoBarDelegate())));
}

PageInfoInfoBarDelegate::PageInfoInfoBarDelegate() = default;

PageInfoInfoBarDelegate::~PageInfoInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
PageInfoInfoBarDelegate::GetIdentifier() const {
  return PAGE_INFO_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& PageInfoInfoBarDelegate::GetVectorIcon() const {
  return vector_icons::kSettingsChromeRefreshIcon;
}

std::u16string PageInfoInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_PAGE_INFO_INFOBAR_TEXT);
}

int PageInfoInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string PageInfoInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_PAGE_INFO_INFOBAR_BUTTON);
}

bool PageInfoInfoBarDelegate::Accept() {
  content::WebContents* web_contents =
      infobars::ContentInfoBarManager::WebContentsFromInfoBar(infobar());
  web_contents->GetController().Reload(content::ReloadType::NORMAL, true);
  return true;
}
