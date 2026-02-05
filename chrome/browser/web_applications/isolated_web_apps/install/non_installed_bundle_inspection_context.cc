// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install/non_installed_bundle_inspection_context.h"

#include "components/webapps/isolated_web_apps/types/source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace web_app {

WEB_CONTENTS_USER_DATA_KEY_IMPL(NonInstalledBundleInspectionContext);

NonInstalledBundleInspectionContext::NonInstalledBundleInspectionContext(
    content::WebContents* web_contents,
    IwaSourceWithMode source,
    IwaOperation operation)
    : content::WebContentsUserData<NonInstalledBundleInspectionContext>(
          *web_contents),
      source_(std::move(source)),
      operation_(std::move(operation)) {}

NonInstalledBundleInspectionContext::~NonInstalledBundleInspectionContext() =
    default;

const IwaSourceWithMode& NonInstalledBundleInspectionContext::source() const {
  return source_;
}

const IwaOperation& NonInstalledBundleInspectionContext::operation() const {
  return operation_;
}

}  // namespace web_app
