// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/test/ssl_test_utils.h"

#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/security_state/core/features.h"
#include "components/security_state/core/security_state.h"

namespace web_app {

void CheckMixedContentLoaded(Browser* browser) {
  DCHECK(browser);
  ssl_test_util::CheckSecurityState(
      browser->tab_strip_model()->GetActiveWebContents(),
      ssl_test_util::CertError::NONE, security_state::WARNING,
      ssl_test_util::AuthState::DISPLAYED_INSECURE_CONTENT);
}

void CheckMixedContentFailedToLoad(Browser* browser) {
  DCHECK(browser);
  ssl_test_util::CheckSecurityState(
      browser->tab_strip_model()->GetActiveWebContents(),
      ssl_test_util::CertError::NONE, security_state::SECURE,
      ssl_test_util::AuthState::NONE);
}

}  // namespace web_app
