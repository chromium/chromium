// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/content/security_state_tab_helper.h"

#include <string>

#include "components/security_state/content/content_utils.h"
#include "content/public/browser/web_contents.h"

using UsesEmbedderInformation = SecurityStateTabHelper::UsesEmbedderInformation;

SecurityStateTabHelper::SecurityStateTabHelper(
    content::WebContents* web_contents,
    UsesEmbedderInformation uses_embedder_information)
    : content::WebContentsUserData<SecurityStateTabHelper>(*web_contents),
      uses_embedder_information_(uses_embedder_information) {}

SecurityStateTabHelper::SecurityStateTabHelper(
    content::WebContents* web_contents)
    : SecurityStateTabHelper(web_contents, UsesEmbedderInformation(false)) {}

SecurityStateTabHelper::~SecurityStateTabHelper() = default;

security_state::SecurityLevel SecurityStateTabHelper::GetSecurityLevel() {
  if (get_security_level_callback_for_tests_) {
    std::move(get_security_level_callback_for_tests_).Run();
  }
  return security_state::GetSecurityLevel(*GetVisibleSecurityState(),
                                          UsedPolicyInstalledCertificate());
}

std::unique_ptr<security_state::VisibleSecurityState>
SecurityStateTabHelper::GetVisibleSecurityState() {
  auto state = security_state::GetVisibleSecurityState(&GetWebContents());

  return state;
}

bool SecurityStateTabHelper::UsedPolicyInstalledCertificate() const {
  return false;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SecurityStateTabHelper);
