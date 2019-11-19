// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_infobar.h"

#include <algorithm>
#include <iterator>

#include "chrome/browser/infobars/infobar_observer.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/browser.h"
#include "components/infobars/core/infobar.h"

TestInfoBar::TestInfoBar() = default;

TestInfoBar::~TestInfoBar() = default;

void TestInfoBar::PreShow() {
  starting_infobars_ = GetNewInfoBars().value();
}

bool TestInfoBar::VerifyUi() {
  base::Optional<InfoBars> infobars = GetNewInfoBars();
  if (!infobars || infobars->empty()) {
    ADD_FAILURE() << "No new infobars were displayed.";
    return false;
  }

  bool expected_infobars_found =
      std::equal(infobars->begin(), infobars->end(),
                 expected_identifiers_.begin(), expected_identifiers_.end(),
                 [](infobars::InfoBar* infobar, InfoBarDelegateIdentifier id) {
                   return infobar->delegate()->GetIdentifier() == id;
                 });
  if (!expected_infobars_found)
    ADD_FAILURE() << "Found unexpected infobars.";

  return expected_infobars_found;
}

void TestInfoBar::WaitForUserDismissal() {
  while (!GetNewInfoBars().value_or(InfoBars()).empty()) {
    InfoBarObserver observer(GetInfoBarService(),
                             InfoBarObserver::Type::kInfoBarRemoved);
    observer.Wait();
  }
}

void TestInfoBar::AddExpectedInfoBar(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  expected_identifiers_.push_back(identifier);
}

content::WebContents* TestInfoBar::GetWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

const content::WebContents* TestInfoBar::GetWebContents() const {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

InfoBarService* TestInfoBar::GetInfoBarService() {
  return const_cast<InfoBarService*>(
      static_cast<const TestInfoBar*>(this)->GetInfoBarService());
}

const InfoBarService* TestInfoBar::GetInfoBarService() const {
  // There may be no web contents if the browser window is closing.
  const content::WebContents* web_contents = GetWebContents();
  return web_contents ? InfoBarService::FromWebContents(web_contents) : nullptr;
}

base::Optional<TestInfoBar::InfoBars> TestInfoBar::GetNewInfoBars() const {
  const InfoBarService* infobar_service = GetInfoBarService();
  if (!infobar_service)
    return base::nullopt;
  const InfoBars& infobars = infobar_service->infobars_;
  if ((infobars.size() < starting_infobars_.size()) ||
      !std::equal(starting_infobars_.begin(), starting_infobars_.end(),
                  infobars.begin()))
    return base::nullopt;
  return InfoBars(std::next(infobars.begin(), starting_infobars_.size()),
                  infobars.end());
}
