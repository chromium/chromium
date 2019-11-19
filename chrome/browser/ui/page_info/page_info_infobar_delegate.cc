// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/page_info_infobar_delegate.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

// static
void PageInfoInfoBarDelegate::Create(InfoBarService* infobar_service) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(new PageInfoInfoBarDelegate())));
}

PageInfoInfoBarDelegate::PageInfoInfoBarDelegate() : ConfirmInfoBarDelegate() {}

PageInfoInfoBarDelegate::~PageInfoInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
PageInfoInfoBarDelegate::GetIdentifier() const {
  return PAGE_INFO_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& PageInfoInfoBarDelegate::GetVectorIcon() const {
  return vector_icons::kSettingsIcon;
}

base::string16 PageInfoInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_PAGE_INFO_INFOBAR_TEXT);
}

int PageInfoInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 PageInfoInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_PAGE_INFO_INFOBAR_BUTTON);
}

bool PageInfoInfoBarDelegate::Accept() {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  web_contents->GetController().Reload(content::ReloadType::NORMAL, true);
  return true;
}
