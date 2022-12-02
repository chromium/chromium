// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/accelerators/accelerator.h"

using BrowserAcceleratorUiTest = InteractiveBrowserTest;

IN_PROC_BROWSER_TEST_F(BrowserAcceleratorUiTest, IncognitoAccelerator) {
  ui::Accelerator incognito_accelerator;
  chrome::AcceleratorProviderForBrowser(browser())->GetAcceleratorForCommandId(
      IDC_NEW_INCOGNITO_WINDOW, &incognito_accelerator);

  RunTestSequence(
      SendAccelerator(kBrowserViewElementId, incognito_accelerator),
      InAnyContext(
          WaitForShow(kBrowserViewElementId).SetTransitionOnlyOnEvent(true)),
      InSameContext(CheckView(
          kBrowserViewElementId, base::BindOnce([](BrowserView* browser) {
            return browser->GetProfile()->IsIncognitoProfile();
          }))));
}
