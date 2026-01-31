// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_installed_watcher.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

ExtensionInstalledWatcher::ExtensionInstalledWatcher(Profile* profile)
    : profile_(profile) {
  extensions::ExtensionRegistry::Get(profile_)->AddObserver(this);
}

ExtensionInstalledWatcher::~ExtensionInstalledWatcher() {
  extensions::ExtensionRegistry::Get(profile_)->RemoveObserver(this);
}

void ExtensionInstalledWatcher::WaitForInstall(
    const extensions::ExtensionId& extension_id,
    base::OnceCallback<void(bool)> done_callback) {
  pending_installs_[extension_id] = std::move(done_callback);

  if (extensions::ExtensionRegistry::Get(profile_)
          ->enabled_extensions()
          .GetByID(extension_id)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ExtensionInstalledWatcher::RunCallback,
                                  weak_factory_.GetWeakPtr(), extension_id,
                                  /*installed=*/true));
  }
}

void ExtensionInstalledWatcher::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  auto it = pending_installs_.find(extension->id());
  if (it != pending_installs_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ExtensionInstalledWatcher::RunCallback,
                                  weak_factory_.GetWeakPtr(), extension->id(),
                                  /*installed=*/true));
  }
}

void ExtensionInstalledWatcher::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  auto it = pending_installs_.find(extension->id());
  if (it != pending_installs_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ExtensionInstalledWatcher::RunCallback,
                                  weak_factory_.GetWeakPtr(), extension->id(),
                                  /*installed=*/false));
  }
}

void ExtensionInstalledWatcher::RunCallback(
    const extensions::ExtensionId& extension_id,
    bool installed) {
  auto it = pending_installs_.find(extension_id);
  if (it == pending_installs_.end()) {
    return;
  }
  if (it->second) {
    base::OnceCallback<void(bool)> callback = std::move(it->second);
    pending_installs_.erase(it);
    std::move(callback).Run(installed);
  } else {
    pending_installs_.erase(it);
  }
}
