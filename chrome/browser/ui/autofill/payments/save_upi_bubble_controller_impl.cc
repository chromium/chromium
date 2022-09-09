// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/save_upi_bubble_controller_impl.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"

namespace autofill {

SaveUPIBubbleControllerImpl::SaveUPIBubbleControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsUserData<SaveUPIBubbleControllerImpl>(*web_contents) {
}

SaveUPIBubbleControllerImpl::~SaveUPIBubbleControllerImpl() = default;

void SaveUPIBubbleControllerImpl::OfferUpiIdLocalSave(
    const std::string& upi_id,
    base::OnceCallback<void(bool accept)> save_upi_prompt_callback) {
  // Don't show the bubble if it's already visible.
  if (save_upi_bubble_)
    return;

  save_upi_prompt_callback_ = std::move(save_upi_prompt_callback);
  upi_id_ = upi_id;
  ShowBubble();
}

std::u16string SaveUPIBubbleControllerImpl::GetUpiId() const {
  return base::UTF8ToUTF16(upi_id_);
}

void SaveUPIBubbleControllerImpl::OnAccept() {
  std::move(save_upi_prompt_callback_).Run(true);
  DCHECK(save_upi_prompt_callback_.is_null());
  upi_id_.clear();
}

void SaveUPIBubbleControllerImpl::OnBubbleClosed() {
  save_upi_bubble_ = nullptr;
}

void SaveUPIBubbleControllerImpl::ShowBubble() {
  DCHECK(!save_upi_bubble_);

  // TODO(crbug.com/986289) Show an icon on the omnibar when saving is proposed.

  Browser* browser = chrome::FindBrowserWithWebContents(&GetWebContents());
  save_upi_bubble_ =
      browser->window()->GetAutofillBubbleHandler()->ShowSaveUPIBubble(
          &GetWebContents(), this);
  DCHECK(save_upi_bubble_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SaveUPIBubbleControllerImpl);

}  // namespace autofill
