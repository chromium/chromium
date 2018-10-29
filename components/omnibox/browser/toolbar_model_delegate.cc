// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/toolbar_model_delegate.h"

bool ToolbarModelDelegate::ShouldDisplayURL() const {
  return true;
}

ToolbarModelDelegate::SecurityLevel ToolbarModelDelegate::GetSecurityLevel()
    const {
  return SecurityLevel::NONE;
}

scoped_refptr<net::X509Certificate> ToolbarModelDelegate::GetCertificate()
    const {
  return nullptr;
}

bool ToolbarModelDelegate::FailsBillingCheck() const {
  return false;
}

bool ToolbarModelDelegate::FailsMalwareCheck() const {
  return false;
}

const gfx::VectorIcon* ToolbarModelDelegate::GetVectorIconOverride() const {
  return nullptr;
}

bool ToolbarModelDelegate::IsOfflinePage() const {
  return false;
}
