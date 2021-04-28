// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_removal_watcher.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"

ExtensionRemovalWatcher::ExtensionRemovalWatcher(
    Browser* browser,
    scoped_refptr<const extensions::Extension> extension,
    base::OnceClosure callback)
    : browser_(browser), extension_(extension), callback_(std::move(callback)) {
  extension_registry_observation_.Observe(
      extensions::ExtensionRegistry::Get(browser->profile()));
  BrowserList::AddObserver(this);
}

ExtensionRemovalWatcher::~ExtensionRemovalWatcher() {
  BrowserList::RemoveObserver(this);
}

void ExtensionRemovalWatcher::OnBrowserClosing(Browser* browser) {
  if (browser == browser_ && callback_)
    std::move(callback_).Run();
}

void ExtensionRemovalWatcher::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  if (extension == extension_.get() && callback_)
    std::move(callback_).Run();
}
