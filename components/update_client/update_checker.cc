// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_checker.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/update_client/component.h"
#include "components/update_client/configurator.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/protocol_serializer.h"
#include "components/update_client/request_sender.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client.h"
#include "components/update_client/updater_state.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

namespace update_client {

namespace {

// Returns a sanitized version of the brand or an empty string otherwise.
std::string SanitizeBrand(const std::string& brand) {
  return IsValidBrand(brand) ? brand : std::string("");
}

// Returns true if at least one item requires network encryption.
bool IsEncryptionRequired(const IdToComponentPtrMap& components) {
  for (const auto& item : components) {
    const auto& component = item.second;
    if (component->crx_component() &&
        component->crx_component()->requires_network_encryption)
      return true;
  }
  return false;
}

// Filters invalid attributes from |installer_attributes|.
using InstallerAttributesFlatMap = base::flat_map<std::string, std::string>;
InstallerAttributesFlatMap SanitizeInstallerAttributes(
    const InstallerAttributes& installer_attributes) {
  InstallerAttributesFlatMap sanitized_attrs;
  for (const auto& attr : installer_attributes) {
    if (IsValidInstallerAttribute(attr))
      sanitized_attrs.insert(attr);
  }
  return sanitized_attrs;
}

class UpdateCheckerImpl : public UpdateChecker {
 public:
  UpdateCheckerImpl(scoped_refptr<Configurator> config,
                    PersistedData* metadata);
  ~UpdateCheckerImpl() override;

  // Overrides for UpdateChecker.
  void CheckForUpdates(
      const std::string& session_id,
      const std::vector<std::string>& ids_checked,
      const IdToComponentPtrMap& components,
      const base::flat_map<std::string, std::string>& additional_attributes,
      bool enabled_component_updates,
      UpdateCheckCallback update_check_callback) override;

 private:
  void ReadUpdaterStateAttributes();
  void CheckForUpdatesHelper(
      const std::string& session_id,
      const IdToComponentPtrMap& components,
      const base::flat_map<std::string, std::string>& additional_attributes,
      bool enabled_component_updates);
  void OnRequestSenderComplete(int error,
                               const std::string& response,
                               int retry_after_sec);
  void UpdateCheckSucceeded(const ProtocolParser::Results& results,
                            int retry_after_sec);
  void UpdateCheckFailed(ErrorCategory error_category,
                         int error,
                         int retry_after_sec);

  base::ThreadChecker thread_checker_;

  const scoped_refptr<Configurator> config_;
  PersistedData* metadata_ = nullptr;
  std::vector<std::string> ids_checked_;
  UpdateCheckCallback update_check_callback_;
  std::unique_ptr<UpdaterState::Attributes> updater_state_attributes_;
  std::unique_ptr<RequestSender> request_sender_;

  DISALLOW_COPY_AND_ASSIGN(UpdateCheckerImpl);
};

UpdateCheckerImpl::UpdateCheckerImpl(scoped_refptr<Configurator> config,
                                     PersistedData* metadata)
    : config_(config), metadata_(metadata) {}

UpdateCheckerImpl::~UpdateCheckerImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void UpdateCheckerImpl::CheckForUpdates(
    const std::string& session_id,
    const std::vector<std::string>& ids_checked,
    const IdToComponentPtrMap& components,
    const base::flat_map<std::string, std::string>& additional_attributes,
    bool enabled_component_updates,
    UpdateCheckCallback update_check_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ids_checked_ = ids_checked;
  update_check_callback_ = std::move(update_check_callback);

  base::PostTaskAndReply(
      FROM_HERE, kTaskTraits,
      base::BindOnce(&UpdateCheckerImpl::ReadUpdaterStateAttributes,
                     base::Unretained(this)),
      base::BindOnce(&UpdateCheckerImpl::CheckForUpdatesHelper,
                     base::Unretained(this), session_id, std::cref(components),
                     additional_attributes, enabled_component_updates));
}

// This function runs on the blocking pool task runner.
void UpdateCheckerImpl::ReadUpdaterStateAttributes() {
#if defined(OS_WIN)
  // On Windows, the Chrome and the updater install modes are matched by design.
  updater_state_attributes_ =
      UpdaterState::GetState(!config_->IsPerUserInstall());
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  // MacOS ignores this value in the current implementation but this may change.
  updater_state_attributes_ = UpdaterState::GetState(false);
#else
// Other platforms don't have updaters.
#endif  // OS_WIN
}

void UpdateCheckerImpl::CheckForUpdatesHelper(
    const std::string& session_id,
    const IdToComponentPtrMap& components,
    const base::flat_map<std::string, std::string>& additional_attributes,
    bool enabled_component_updates) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto urls(config_->UpdateUrl());
  if (IsEncryptionRequired(components))
    RemoveUnsecureUrls(&urls);

  // Components in this update check are either all foreground, or all
  // background since this member is inherited from the component's update
  // context. Pick the state of the first component to use in the update check.
  DCHECK(!components.empty());
  const bool is_foreground = components.at(ids_checked_[0])->is_foreground();
  DCHECK(
      std::all_of(components.cbegin(), components.cend(),
                  [is_foreground](IdToComponentPtrMap::const_reference& elem) {
                    return is_foreground == elem.second->is_foreground();
                  }));

  std::vector<protocol_request::App> apps;
  for (const auto& app_id : ids_checked_) {
    DCHECK_EQ(1u, components.count(app_id));
    const auto& component = components.at(app_id);
    DCHECK_EQ(component->id(), app_id);
    const auto& crx_component = component->crx_component();
    DCHECK(crx_component);

    std::string install_source;
    if (!crx_component->install_source.empty())
      install_source = crx_component->install_source;
    else if (component->is_foreground())
      install_source = "ondemand";

    const bool is_update_disabled =
        crx_component->supports_group_policy_enable_component_updates &&
        !enabled_component_updates;

    apps.push_back(MakeProtocolApp(
        app_id, crx_component->version, SanitizeBrand(config_->GetBrand()),
        install_source, crx_component->install_location,
        crx_component->fingerprint,
        SanitizeInstallerAttributes(crx_component->installer_attributes),
        metadata_->GetCohort(app_id), metadata_->GetCohortName(app_id),
        metadata_->GetCohortHint(app_id), crx_component->disabled_reasons,
        MakeProtocolUpdateCheck(is_update_disabled),
        MakeProtocolPing(app_id, metadata_)));
  }

  const auto request = MakeProtocolRequest(
      session_id, config_->GetProdId(),
      config_->GetBrowserVersion().GetString(), config_->GetLang(),
      config_->GetChannel(), config_->GetOSLongName(),
      config_->GetDownloadPreference(), additional_attributes,
      updater_state_attributes_.get(), std::move(apps));

  request_sender_ = std::make_unique<RequestSender>(config_);
  request_sender_->Send(
      urls,
      BuildUpdateCheckExtraRequestHeaders(config_->GetProdId(),
                                          config_->GetBrowserVersion(),
                                          ids_checked_, is_foreground),
      config_->GetProtocolHandlerFactory()->CreateSerializer()->Serialize(
          request),
      config_->EnabledCupSigning(),
      base::BindOnce(&UpdateCheckerImpl::OnRequestSenderComplete,
                     base::Unretained(this)));
}

void UpdateCheckerImpl::OnRequestSenderComplete(
    int error,
    const std::string& response,
    int retry_after_sec) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (error) {
    VLOG(1) << "RequestSender failed " << error;
    UpdateCheckFailed(ErrorCategory::kUpdateCheck, error, retry_after_sec);
    return;
  }

  auto parser = config_->GetProtocolHandlerFactory()->CreateParser();
  if (!parser->Parse(response)) {
    VLOG(1) << "Parse failed " << parser->errors();
    UpdateCheckFailed(ErrorCategory::kUpdateCheck,
                      static_cast<int>(ProtocolError::PARSE_FAILED),
                      retry_after_sec);
    return;
  }

  DCHECK_EQ(0, error);
  UpdateCheckSucceeded(parser->results(), retry_after_sec);
}

void UpdateCheckerImpl::UpdateCheckSucceeded(
    const ProtocolParser::Results& results,
    int retry_after_sec) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const int daynum = results.daystart_elapsed_days;
  if (daynum != ProtocolParser::kNoDaystart) {
    metadata_->SetDateLastActive(ids_checked_, daynum);
    metadata_->SetDateLastRollCall(ids_checked_, daynum);
  }
  for (const auto& result : results.list) {
    auto entry = result.cohort_attrs.find(ProtocolParser::Result::kCohort);
    if (entry != result.cohort_attrs.end())
      metadata_->SetCohort(result.extension_id, entry->second);
    entry = result.cohort_attrs.find(ProtocolParser::Result::kCohortName);
    if (entry != result.cohort_attrs.end())
      metadata_->SetCohortName(result.extension_id, entry->second);
    entry = result.cohort_attrs.find(ProtocolParser::Result::kCohortHint);
    if (entry != result.cohort_attrs.end())
      metadata_->SetCohortHint(result.extension_id, entry->second);
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(update_check_callback_),
                     base::make_optional<ProtocolParser::Results>(results),
                     ErrorCategory::kNone, 0, retry_after_sec));
}

void UpdateCheckerImpl::UpdateCheckFailed(ErrorCategory error_category,
                                          int error,
                                          int retry_after_sec) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(0, error);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(update_check_callback_), base::nullopt,
                     error_category, error, retry_after_sec));
}

}  // namespace

std::unique_ptr<UpdateChecker> UpdateChecker::Create(
    scoped_refptr<Configurator> config,
    PersistedData* persistent) {
  return std::make_unique<UpdateCheckerImpl>(config, persistent);
}

}  // namespace update_client
