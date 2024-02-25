// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_PAGE_INFO_UI_DELEGATE_H_
#define COMPONENTS_PAGE_INFO_PAGE_INFO_UI_DELEGATE_H_

#include <optional>

#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/permission_result.h"

namespace blink {
enum class PermissionType;
}

class PageInfoUiDelegate {
 public:
  virtual ~PageInfoUiDelegate() = default;
#if !BUILDFLAG(IS_ANDROID)
  virtual bool IsBlockAutoPlayEnabled() = 0;
  virtual bool IsMultipleTabsOpen() = 0;
  virtual void OpenSiteSettingsFileSystem() = 0;
#endif
  // This function is temporarily needed while rolling out 3PCD.
  virtual bool IsTrackingProtection3pcdEnabled() = 0;
  virtual content::PermissionResult GetPermissionResult(
      blink::PermissionType permission) = 0;
  virtual std::optional<content::PermissionResult> GetEmbargoResult(
      ContentSettingsType type) = 0;
};

#endif  // COMPONENTS_PAGE_INFO_PAGE_INFO_UI_DELEGATE_H_
