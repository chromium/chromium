// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_list_component_updater.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/win/conflicts/module_database.h"
#include "components/update_client/crx_update_item.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

ModuleListComponentUpdater::~ModuleListComponentUpdater() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

// static
ModuleListComponentUpdater::UniquePtr ModuleListComponentUpdater::Create(
    const std::string& module_list_component_id,
    const base::RepeatingClosure&
        on_module_list_component_not_updated_callback) {
  return UniquePtr(
      new ModuleListComponentUpdater(
          module_list_component_id,
          on_module_list_component_not_updated_callback),
      base::OnTaskRunnerDeleter(content::GetUIThreadTaskRunner({})));
}

ModuleListComponentUpdater::ModuleListComponentUpdater(
    const std::string& module_list_component_id,
    const base::RepeatingClosure& on_module_list_component_not_updated_callback)
    : module_list_component_id_(module_list_component_id),
      on_module_list_component_not_updated_callback_(
          on_module_list_component_not_updated_callback) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ModuleListComponentUpdater::InitializeOnUIThread,
                     base::Unretained(this)));
}

void ModuleListComponentUpdater::InitializeOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* component_update_service = g_browser_process->component_updater();

  // Observe the component updater service to know the result of the update.
  observation_.Observe(component_update_service);

  component_update_service->MaybeThrottle(module_list_component_id_,
                                          base::DoNothing());
}

void ModuleListComponentUpdater::OnEvent(
    const update_client::CrxUpdateItem& item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!module_list_component_id_.empty());

  // Only consider events for the module list component.
  if (item.id != module_list_component_id_) {
    return;
  }

  // There are 2 cases that are important. Either the component is being
  // updated, or the component is not updated either because there is no update
  // available or the update failed.
  //
  // For the first case, there is nothing to do because LoadModuleList() will
  // eventually be called when the component is installed.
  //
  // For the second case, it means that the server is not offering any update
  // right now, either because it is too busy, or there is an issue with the
  // server-side component configuration.

  // Note: This class only registers as an observer when the component is not
  // installed, so kUpToDate should be rare. However, it may occur if there is
  // no compatible version to install, or if the update server is overloaded.
  // It is safe to treat this as an error.

  if (item.state == update_client::ComponentState::kUpToDate ||
      item.state == update_client::ComponentState::kUpdateError) {
    ModuleDatabase::GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(on_module_list_component_not_updated_callback_));
  }
}
