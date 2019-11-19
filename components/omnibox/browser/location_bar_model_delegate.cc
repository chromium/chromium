// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model_delegate.h"

bool LocationBarModelDelegate::ShouldPreventElision() const {
  return false;
}

bool LocationBarModelDelegate::ShouldDisplayURL() const {
  return true;
}

security_state::SecurityLevel LocationBarModelDelegate::GetSecurityLevel()
    const {
  return security_state::NONE;
}

std::unique_ptr<security_state::VisibleSecurityState>
LocationBarModelDelegate::GetVisibleSecurityState() const {
  return std::make_unique<security_state::VisibleSecurityState>();
}

scoped_refptr<net::X509Certificate> LocationBarModelDelegate::GetCertificate()
    const {
  return nullptr;
}

const gfx::VectorIcon* LocationBarModelDelegate::GetVectorIconOverride() const {
  return nullptr;
}

bool LocationBarModelDelegate::IsOfflinePage() const {
  return false;
}

bool LocationBarModelDelegate::IsInstantNTP() const {
  return false;
}

bool LocationBarModelDelegate::IsNewTabPage(const GURL& url) const {
  return false;
}

bool LocationBarModelDelegate::IsHomePage(const GURL& url) const {
  return false;
}

AutocompleteClassifier* LocationBarModelDelegate::GetAutocompleteClassifier() {
  return nullptr;
}

TemplateURLService* LocationBarModelDelegate::GetTemplateURLService() {
  return nullptr;
}
