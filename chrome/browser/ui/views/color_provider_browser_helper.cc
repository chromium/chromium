// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/color_provider_browser_helper.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents.h"
#include "ui/color/color_provider_source.h"

DEFINE_USER_DATA(ColorProviderBrowserHelper);

void ColorProviderBrowserHelper::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() != TabStripModelChange::kInserted) {
    return;
  }
  for (const auto& contents : change.GetInsert()->contents) {
    CHECK(tab_strip_model->ContainsIndex(contents.index));
    contents.contents->SetColorProviderSource(color_provider_source_);
  }
}

ColorProviderBrowserHelper::ColorProviderBrowserHelper(
    TabStripModel* tab_strip_model,
    ui::ColorProviderSource* color_provider_source,
    BrowserWindowInterface* browser)
    : tab_strip_model_(tab_strip_model),
      color_provider_source_(color_provider_source),
      scoped_data_holder_(browser->GetUnownedUserDataHost(), *this) {
  CHECK(tab_strip_model);
  CHECK(color_provider_source);
  // No WebContents should have been added to the TabStripModel before
  // ColorProviderBrowserHelper is constructed.
  CHECK(tab_strip_model->empty());
  tab_strip_model->AddObserver(this);
}

ColorProviderBrowserHelper::~ColorProviderBrowserHelper() = default;

ColorProviderBrowserHelper* ColorProviderBrowserHelper::From(
    BrowserWindowInterface* browser_window_interface) {
  return Get(browser_window_interface->GetUnownedUserDataHost());
}
