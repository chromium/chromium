// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/test_location_bar_model.h"

#include "base/strings/utf_string_conversions.h"

#if defined(TOOLKIT_VIEWS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif

TestLocationBarModel::TestLocationBarModel()
    : security_level_(security_state::NONE),
#if defined(TOOLKIT_VIEWS)
      icon_(&omnibox::kHttpIcon),
#endif
      should_display_url_(true) {
}

TestLocationBarModel::~TestLocationBarModel() = default;

std::u16string TestLocationBarModel::GetFormattedFullURL() const {
  if (!formatted_full_url_)
    return base::UTF8ToUTF16(url_.spec());

  return *formatted_full_url_;
}

std::u16string TestLocationBarModel::GetURLForDisplay() const {
  if (!url_for_display_)
    return base::UTF8ToUTF16(url_.spec());

  return *url_for_display_;
}

GURL TestLocationBarModel::GetURL() const {
  return url_;
}

security_state::SecurityLevel TestLocationBarModel::GetSecurityLevel() const {
  return security_level_;
}

net::CertStatus TestLocationBarModel::GetCertStatus() const {
  return cert_status_;
}

metrics::OmniboxEventProto::PageClassification
TestLocationBarModel::GetPageClassification(bool is_prefetch) const {
  return metrics::OmniboxEventProto::OTHER;
}

const gfx::VectorIcon& TestLocationBarModel::GetVectorIcon() const {
  return *icon_;
}

std::u16string TestLocationBarModel::GetSecureDisplayText() const {
  return secure_display_text_;
}

std::u16string TestLocationBarModel::GetSecureAccessibilityText() const {
  return std::u16string();
}

bool TestLocationBarModel::ShouldDisplayURL() const {
  return should_display_url_;
}

bool TestLocationBarModel::IsOfflinePage() const {
  return offline_page_;
}

bool TestLocationBarModel::ShouldPreventElision() const {
  return should_prevent_elision_;
}

bool TestLocationBarModel::ShouldUseUpdatedConnectionSecurityIndicators()
    const {
  return false;
}
