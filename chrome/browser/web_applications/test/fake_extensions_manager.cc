// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_extensions_manager.h"

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/web_applications/extensions_manager.h"

namespace web_app {
namespace {
class FakeExtensionInstallGate : public ExtensionInstallGate {};
}  // namespace

FakeExtensionsManager::FakeExtensionsManager() = default;
FakeExtensionsManager::~FakeExtensionsManager() = default;

void FakeExtensionsManager::SetExtensionsSytemReady(bool ready) {
  extensions_system_ready_ = true;
  for (base::OnceClosure& closure : ready_waiters_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(closure));
  }
  ready_waiters_.clear();
}
void FakeExtensionsManager::SetIsolatedStoragePaths(
    std::unordered_set<base::FilePath> paths) {
  isolated_storage_paths_ = paths;
}

void FakeExtensionsManager::OnExtensionSystemReady(base::OnceClosure closure) {
  if (extensions_system_ready_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(closure));
    return;
  }
  ready_waiters_.push_back(std::move(closure));
}
std::unordered_set<base::FilePath>
FakeExtensionsManager::GetIsolatedStoragePaths() {
  return isolated_storage_paths_;
}
std::unique_ptr<ExtensionInstallGate>
FakeExtensionsManager::RegisterGarbageCollectionInstallGate() {
  return std::make_unique<FakeExtensionInstallGate>();
}
}  // namespace web_app
