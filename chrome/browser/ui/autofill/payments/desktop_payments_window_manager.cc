// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/desktop_payments_window_manager.h"

#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace autofill::payments {

DesktopPaymentsWindowManager::DesktopPaymentsWindowManager(
    ChromeAutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

DesktopPaymentsWindowManager::~DesktopPaymentsWindowManager() = default;

void DesktopPaymentsWindowManager::CreatePopup(const GURL& url) {
  // Create a pop-up window. The created pop-up will not have any relationship
  // to the underlying tab, because `params.opener` is not set. Ensuring the
  // original tab is not a related site instance to the pop-up is critical for
  // security reasons.
  NavigateParams params(
      Profile::FromBrowserContext(client_->web_contents()->GetBrowserContext()),
      url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.source_contents = client_->web_contents();
  params.is_tab_modal_popup = true;

  // TODO(crbug.com/1517762): Handle the case where the pop-up is not shown by
  // displaying an error message.
  Navigate(&params);
}

}  // namespace autofill::payments
