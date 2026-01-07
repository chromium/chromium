// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

#include "base/no_destructor.h"
#include "base/notimplemented.h"

GlobalBrowserCollection::GlobalBrowserCollection() = default;

GlobalBrowserCollection::~GlobalBrowserCollection() = default;

// static
GlobalBrowserCollection* GlobalBrowserCollection::GetInstance() {
  // TODO(crbug.com/474120522): Find somewhere else to put this, like
  // GlobalFeatures, rather than having static scope.
  NOTIMPLEMENTED();

  static base::NoDestructor<GlobalBrowserCollection> instance;
  return instance.get();
}

bool GlobalBrowserCollection::IsEmpty() const {
  // TODO(crbug.com/474120522): implement
  NOTIMPLEMENTED();
  return false;
}

size_t GlobalBrowserCollection::GetSize() const {
  // TODO(crbug.com/474120522): implement
  NOTIMPLEMENTED();
  return 0;
}

BrowserCollection::BrowserVector GlobalBrowserCollection::GetBrowsers(
    Order order) {
  // TODO(crbug.com/474120522): implement
  NOTIMPLEMENTED();
  return {};
}

void GlobalBrowserCollection::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  // TODO(crbug.com/474120522): implement
  NOTIMPLEMENTED();
}

void GlobalBrowserCollection::OnBrowserClosed(BrowserWindowInterface* browser) {
  // TODO(crbug.com/474120522): implement
  NOTIMPLEMENTED();
}

void GlobalBrowserCollection::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  // TODO(crbug.com/474120522): implement
  NOTIMPLEMENTED();
}

void GlobalBrowserCollection::OnBrowserDeactivated(
    BrowserWindowInterface* browser) {
  // TODO(crbug.com/474120522): implement
  NOTIMPLEMENTED();
}
