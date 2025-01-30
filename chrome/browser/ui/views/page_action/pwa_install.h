// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PWA_INSTALL_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PWA_INSTALL_H_

#include "chrome/browser/ui/browser.h"
#include "content/public/browser/web_contents.h"

void ShowPwaInstallDialog(Browser* browser, content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PWA_INSTALL_H_
