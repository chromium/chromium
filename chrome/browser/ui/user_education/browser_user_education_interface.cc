// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/browser_user_education_interface.h"

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

DEFINE_USER_DATA(BrowserUserEducationInterface);

BrowserUserEducationInterface::BrowserUserEducationInterface(
    BrowserWindowInterface* browser)
    : scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {}
BrowserUserEducationInterface::~BrowserUserEducationInterface() = default;

void BrowserUserEducationInterface::Init(BrowserView*) {}
void BrowserUserEducationInterface::TearDown() {}

// static
BrowserUserEducationInterface*
BrowserUserEducationInterface::MaybeGetForWebContentsInTab(
    content::WebContents* contents) {
  auto* const tab = tabs::TabInterface::MaybeGetFromContents(contents);
  return tab ? From(tab->GetBrowserWindowInterface()) : nullptr;
}

// static
BrowserUserEducationInterface* BrowserUserEducationInterface::From(
    BrowserWindowInterface* browser) {
  return browser ? Get(browser->GetUnownedUserDataHost()) : nullptr;
}
