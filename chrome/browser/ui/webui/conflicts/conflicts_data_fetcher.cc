// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/conflicts/conflicts_data_fetcher.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/strings/string_util.h"
#include "chrome/browser/win/conflicts/module_database.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Converts the process_types bit field to a simple string representation where
// each process type is represented by a letter. E.g. B for browser process.
// Full process names are not used in order to save horizontal space in the
// conflicts UI.
std::string GetProcessTypesString(const ModuleInfoData& module_data) {
  uint32_t process_types = module_data.process_types;

  if (!process_types) {
    return "None";
  }

  std::string result;
  if (process_types & ProcessTypeToBit(content::PROCESS_TYPE_BROWSER)) {
    result.append("B");
  }
  if (process_types & ProcessTypeToBit(content::PROCESS_TYPE_RENDERER)) {
    result.append("R");
  }
  // TODO(pmonette): Add additional process types as more get supported.

  return result;
}

}  // namespace

ConflictsDataFetcher::~ConflictsDataFetcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (module_list_.has_value()) {
    ModuleDatabase::GetInstance()->RemoveObserver(this);
  }
}

// static
ConflictsDataFetcher::UniquePtr ConflictsDataFetcher::Create(
    OnConflictsDataFetchedCallback on_conflicts_data_fetched_callback) {
  return std::unique_ptr<ConflictsDataFetcher, base::OnTaskRunnerDeleter>(
      new ConflictsDataFetcher(std::move(on_conflicts_data_fetched_callback)),
      base::OnTaskRunnerDeleter(ModuleDatabase::GetTaskRunner()));
}

ConflictsDataFetcher::ConflictsDataFetcher(
    OnConflictsDataFetchedCallback on_conflicts_data_fetched_callback)
    : on_conflicts_data_fetched_callback_(
          std::move(on_conflicts_data_fetched_callback)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  ModuleDatabase::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ConflictsDataFetcher::InitializeOnModuleDatabaseTaskRunner,
          base::Unretained(this)));
}

void ConflictsDataFetcher::InitializeOnModuleDatabaseTaskRunner() {
  GetListOfModules();
}

void ConflictsDataFetcher::GetListOfModules() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The request is handled asynchronously, filling up the |module_list_|,
  // and will callback via OnModuleDatabaseIdle() on completion.
  module_list_ = base::Value::List();

  auto* module_database = ModuleDatabase::GetInstance();
  module_database->StartInspection();
  module_database->AddObserver(this);
}

void ConflictsDataFetcher::OnNewModuleFound(const ModuleInfoKey& module_key,
                                            const ModuleInfoData& module_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(module_list_);

  base::Value::Dict data;

  std::string type_string;
  if (module_data.module_properties & ModuleInfoData::kPropertyShellExtension) {
    type_string = "Shell extension";
  }
  data.Set("type_description", type_string);

  const auto& inspection_result = *module_data.inspection_result;
  data.Set("location", inspection_result.location);
  data.Set("name", inspection_result.basename);
  data.Set("product_name", inspection_result.product_name);
  data.Set("description", inspection_result.description);
  data.Set("version", inspection_result.version);
  data.Set("digital_signer", inspection_result.certificate_info.subject);
  data.Set("code_id", GenerateCodeId(module_key));
  data.Set("process_types", GetProcessTypesString(module_data));

  module_list_->Append(std::move(data));
}

void ConflictsDataFetcher::OnModuleDatabaseIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(module_list_);

  ModuleDatabase::GetInstance()->RemoveObserver(this);

  base::Value::Dict results;
  results.Set("moduleCount", static_cast<int>(module_list_->size()));
  results.Set("moduleList", std::move(*module_list_));
  module_list_ = std::nullopt;

  // The third-party features are always disabled on Chromium builds.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_conflicts_data_fetched_callback_),
                                std::move(results)));
}
