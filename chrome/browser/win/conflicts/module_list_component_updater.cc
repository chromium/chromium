// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_list_component_updater.h"

#include <utility>

#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/win/conflicts/module_database.h"
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
  return UniquePtr(new ModuleListComponentUpdater(
                       module_list_component_id,
                       on_module_list_component_not_updated_callback),
                   base::OnTaskRunnerDeleter(base::CreateSequencedTaskRunner(
                       {content::BrowserThread::UI})));
}

ModuleListComponentUpdater::ModuleListComponentUpdater(
    const std::string& module_list_component_id,
    const base::RepeatingClosure& on_module_list_component_not_updated_callback)
    : module_list_component_id_(module_list_component_id),
      on_module_list_component_not_updated_callback_(
          on_module_list_component_not_updated_callback),
      observer_(this) {
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&ModuleListComponentUpdater::InitializeOnUIThread,
                     base::Unretained(this)));
}

void ModuleListComponentUpdater::InitializeOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* component_update_service = g_browser_process->component_updater();

  // Observe the component updater service to know the result of the update.
  observer_.Add(component_update_service);

  component_update_service->MaybeThrottle(module_list_component_id_,
                                          base::DoNothing());
}

void ModuleListComponentUpdater::OnEvent(Events event,
                                         const std::string& component_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!module_list_component_id_.empty());

  // Only consider events for the module list component.
  if (component_id != module_list_component_id_)
    return;

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
  //
  // Note:
  // The COMPONENT_NOT_UPDATED event can also be broadcasted when the component
  // is already up-to-date. This is not the case here because this class only
  // registers to the component updater service as an observer when the
  // component version is 0.0.0.0 (aka not installed).
  if (event == Events::COMPONENT_NOT_UPDATED ||
      event == Events::COMPONENT_UPDATE_ERROR) {
    ModuleDatabase::GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(on_module_list_component_not_updated_callback_));
  }
}
