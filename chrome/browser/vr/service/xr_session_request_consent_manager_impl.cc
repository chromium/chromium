// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/xr_session_request_consent_manager_impl.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/xr/xr_session_request_consent_dialog_delegate.h"
#include "content/public/browser/web_contents.h"

namespace vr {

XRSessionRequestConsentManagerImpl::XRSessionRequestConsentManagerImpl() =
    default;

XRSessionRequestConsentManagerImpl::~XRSessionRequestConsentManagerImpl() =
    default;

TabModalConfirmDialog*
XRSessionRequestConsentManagerImpl::ShowDialogAndGetConsent(
    content::WebContents* web_contents,
    XrConsentPromptLevel consent_level,
    base::OnceCallback<void(XrConsentPromptLevel, bool)> response_callback) {
  auto delegate = std::make_unique<XrSessionRequestConsentDialogDelegate>(
      web_contents, consent_level, std::move(response_callback));
  delegate->OnShowDialog();
  return TabModalConfirmDialog::Create(std::move(delegate), web_contents);
}

}  // namespace vr
