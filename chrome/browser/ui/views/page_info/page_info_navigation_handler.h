// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_NAVIGATION_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_NAVIGATION_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "components/content_settings/core/common/content_settings_types.h"

// An interface that provides methods to navigate between pages of the page
// info. Note that `OpenMainPage` must update the set of ignored empty storage
// keys before storage usage can be displayed. This happens asynchronously and
// `initialized_callback` fires after it has finished.
class PageInfoNavigationHandler {
 public:
  virtual void OpenMainPage(base::OnceClosure initialized_callback) = 0;
  virtual void OpenSecurityPage() = 0;
  virtual void OpenPermissionPage(ContentSettingsType type) = 0;
  virtual void OpenAdPersonalizationPage() = 0;
  virtual void OpenCookiesPage() = 0;
  virtual void CloseBubble() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_NAVIGATION_HANDLER_H_
