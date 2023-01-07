// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/registry_monitor.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/registry.h"
#include "chrome/browser/upgrade_detector/installed_version_monitor.h"
#include "chrome/install_static/install_util.h"

namespace {

base::win::RegKey GetDefaultMonitorLocation() {
  return base::win::RegKey(install_static::IsSystemInstall()
                               ? HKEY_LOCAL_MACHINE
                               : HKEY_CURRENT_USER,
                           install_static::GetClientsKeyPath().c_str(),
                           KEY_NOTIFY | KEY_WOW64_32KEY);
}

}  // namespace

RegistryMonitor::RegistryMonitor(base::win::RegKey key)
    : clients_key_(std::move(key)) {}

RegistryMonitor::~RegistryMonitor() = default;

void RegistryMonitor::Start(Callback on_change_callback) {
  DCHECK(on_change_callback);
  DCHECK(!on_change_callback_);
  on_change_callback_ = std::move(on_change_callback);
  StartWatching();
}

void RegistryMonitor::StartWatching() {
  // base::Unretained is safe because RegistryMonitor owns the RegKey.
  // Destruction of this instance will cancel the watch and disable any
  // outstanding notifications.
  if (!clients_key_.Valid() ||
      !clients_key_.StartWatching(base::BindOnce(
          &RegistryMonitor::OnClientsKeyChanged, base::Unretained(this)))) {
    // Starting the watch failed. Report this back to the poller via a delayed
    // task.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(on_change_callback_, /*error=*/true));
  }
}

void RegistryMonitor::OnClientsKeyChanged() {
  on_change_callback_.Run(/*error=*/false);
  StartWatching();
}

// static
std::unique_ptr<InstalledVersionMonitor> InstalledVersionMonitor::Create() {
  return std::make_unique<RegistryMonitor>(GetDefaultMonitorLocation());
}
