// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_installed_waiter.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"

namespace {
base::RepeatingClosure* g_giving_up_callback = nullptr;
}  // namespace

// static
void ExtensionInstalledWaiter::WaitForInstall(
    scoped_refptr<const extensions::Extension> extension,
    Browser* browser,
    base::OnceClosure done_callback) {
  (new ExtensionInstalledWaiter(extension, browser, std::move(done_callback)))
      ->RunCallbackIfExtensionInstalled();
}

void ExtensionInstalledWaiter::SetGivingUpCallbackForTesting(
    base::RepeatingClosure callback) {
  if (g_giving_up_callback)
    delete g_giving_up_callback;
  if (!callback.is_null())
    g_giving_up_callback = new base::RepeatingClosure(callback);
  else
    g_giving_up_callback = nullptr;
}

ExtensionInstalledWaiter::ExtensionInstalledWaiter(
    scoped_refptr<const extensions::Extension> extension,
    Browser* browser,
    base::OnceClosure done_callback)
    : extension_(extension),
      browser_(browser),
      done_callback_(std::move(done_callback)) {
  extension_registry_observation_.Observe(
      extensions::ExtensionRegistry::Get(browser->profile()));
  BrowserList::AddObserver(this);
}

ExtensionInstalledWaiter::~ExtensionInstalledWaiter() {
  if (done_callback_ && g_giving_up_callback)
    g_giving_up_callback->Run();
  BrowserList::RemoveObserver(this);
}

void ExtensionInstalledWaiter::RunCallbackIfExtensionInstalled() {
  if (IsExtensionInstalled()) {
    std::move(done_callback_).Run();
    delete this;
    return;
  }
}

bool ExtensionInstalledWaiter::IsExtensionInstalled() const {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_->profile());
  return registry->enabled_extensions().GetByID(extension_->id());
}

void ExtensionInstalledWaiter::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (extension != extension_.get())
    return;

  // Only call Wait() after all the other extension observers have had a chance
  // to run.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ExtensionInstalledWaiter::RunCallbackIfExtensionInstalled,
                     weak_factory_.GetWeakPtr()));
}

void ExtensionInstalledWaiter::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  if (extension == extension_.get())
    delete this;
}

void ExtensionInstalledWaiter::OnBrowserRemoved(Browser* browser) {
  if (browser == browser_)
    delete this;
}
