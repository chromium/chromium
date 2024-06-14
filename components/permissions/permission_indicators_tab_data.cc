// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_indicators_tab_data.h"

#include "components/permissions/permission_uma_util.h"
#include "content/public/browser/web_contents.h"

namespace permissions {

PermissionIndicatorsTabData::PermissionIndicatorsTabData(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK(web_contents);
  origin_ = web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
}

PermissionIndicatorsTabData::~PermissionIndicatorsTabData() = default;

bool PermissionIndicatorsTabData::IsVerboseIndicatorAllowed(
    IndicatorsType type) const {
  return !displayed_indicators_.contains(type);
}

void PermissionIndicatorsTabData::SetVerboseIndicatorDisplayed(
    IndicatorsType type) {
  displayed_indicators_.insert(type);
}

void PermissionIndicatorsTabData::ClearData() {
  displayed_indicators_.clear();
}

void PermissionIndicatorsTabData::WebContentsDestroyed() {
  ClearData();
}

void PermissionIndicatorsTabData::PrimaryPageChanged(content::Page& page) {
  if (origin_ != page.GetMainDocument().GetLastCommittedOrigin()) {
    origin_ = page.GetMainDocument().GetLastCommittedOrigin();
    ClearData();
  }
}
}  // namespace permissions
