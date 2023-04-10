// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_CHROME_JAVASCRIPT_APP_MODAL_DIALOG_VIEW_FACTORY_H_
#define CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_CHROME_JAVASCRIPT_APP_MODAL_DIALOG_VIEW_FACTORY_H_

#include "build/build_config.h"

void InstallChromeJavaScriptAppModalDialogViewFactory();

#if BUILDFLAG(IS_MAC)
void InstallChromeJavaScriptAppModalDialogViewCocoaFactory();
#endif

void SetChromeAppModalDialogManagerDelegate();

#endif  // CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_CHROME_JAVASCRIPT_APP_MODAL_DIALOG_VIEW_FACTORY_H_
