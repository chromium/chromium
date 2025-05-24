// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_MODULE_LIST_COMPONENT_UPDATER_H_
#define CHROME_BROWSER_WIN_CONFLICTS_MODULE_LIST_COMPONENT_UPDATER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "components/component_updater/component_updater_service.h"

// This class takes care of updating the module list component. A successful
// update will cause the ThirdPartyModuleListComponentInstaller class to call
// ModuleDatabase::LoadModuleList(). If the update was unsuccessful, the
// |on_module_list_component_not_updated_callback_| will be invoked instead.
class ModuleListComponentUpdater
    : public component_updater::ComponentUpdateService::Observer {
 public:
  using UniquePtr =
      std::unique_ptr<ModuleListComponentUpdater, base::OnTaskRunnerDeleter>;

  ModuleListComponentUpdater(const ModuleListComponentUpdater&) = delete;
  ModuleListComponentUpdater& operator=(const ModuleListComponentUpdater&) =
      delete;

  ~ModuleListComponentUpdater() override;

  // Creates a new instance that lives on the UI thread.
  static UniquePtr Create(const std::string& module_list_component_id,
                          const base::RepeatingClosure&
                              on_module_list_component_not_updated_callback);

 private:
  ModuleListComponentUpdater(const std::string& module_list_component_id,
                             const base::RepeatingClosure&
                                 on_module_list_component_not_updated_callback);

  void InitializeOnUIThread();

  // ComponentUpdateService::Observer:
  void OnEvent(const update_client::CrxUpdateItem& item) override;

  // Holds the id of the Third Party Module List component.
  std::string module_list_component_id_;

  base::RepeatingClosure on_module_list_component_not_updated_callback_;

  // Observes the component update service when an update to the Module List
  // component was forced.
  base::ScopedObservation<component_updater::ComponentUpdateService,
                          component_updater::ComponentUpdateService::Observer>
      observation_{this};
};

#endif  // CHROME_BROWSER_WIN_CONFLICTS_MODULE_LIST_COMPONENT_UPDATER_H_
