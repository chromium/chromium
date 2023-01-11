// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_checker.h"

#include <stddef.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/component.h"
#include "components/update_client/configurator.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/protocol_serializer.h"
#include "components/update_client/request_sender.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_engine.h"
#include "components/update_client/utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace update_client {

namespace {

class UpdateCheckerImpl : public UpdateChecker {
 public:
  UpdateCheckerImpl(scoped_refptr<Configurator> config,
                    PersistedData* metadata);

  UpdateCheckerImpl(const UpdateCheckerImpl&) = delete;
  UpdateCheckerImpl& operator=(const UpdateCheckerImpl&) = delete;

  ~UpdateCheckerImpl() override;

  // Overrides for UpdateChecker.
  void CheckForUpdates(
      scoped_refptr<UpdateContext> context,
      const base::flat_map<std::string, std::string>& additional_attributes,
      UpdateCheckCallback update_check_callback) override;

 private:
  UpdaterStateAttributes ReadUpdaterStateAttributes() const;
  void CheckForUpdatesHelper(
      scoped_refptr<UpdateContext> context,
      const std::vector<GURL>& urls,
      const base::flat_map<std::string, std::string>& additional_attributes,
      const UpdaterStateAttributes& updater_state_attributes,
      const std::set<std::string>& active_ids);
  void OnRequestSenderComplete(scoped_refptr<UpdateContext> context,
                               absl::optional<base::OnceClosure> fallback,
                               int error,
                               const std::string& response,
                               int retry_after_sec);
  void UpdateCheckSucceeded(scoped_refptr<UpdateContext> context,
                            const ProtocolParser::Results& results,
                            int retry_after_sec);
  void UpdateCheckFailed(ErrorCategory error_category,
                         int error,
                         int retry_after_sec);

  base::ThreadChecker thread_checker_;

  const scoped_refptr<Configurator> config_;
  raw_ptr<PersistedData> metadata_ = nullptr;
  UpdateCheckCallback update_check_callback_;
  std::unique_ptr<RequestSender> request_sender_;
};

UpdateCheckerImpl::UpdateCheckerImpl(scoped_refptr<Configurator> config,
                                     PersistedData* metadata)
    : config_(config), metadata_(metadata) {}

UpdateCheckerImpl::~UpdateCheckerImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void UpdateCheckerImpl::CheckForUpdates(
    scoped_refptr<UpdateContext> context,
    const base::flat_map<std::string, std::string>& additional_attributes,
    UpdateCheckCallback update_check_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  update_check_callback_ = std::move(update_check_callback);

  auto check_for_updates_invoker = base::BindOnce(
      &UpdateCheckerImpl::CheckForUpdatesHelper, base::Unretained(this),
      context, config_->UpdateUrl(), additional_attributes);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(&UpdateCheckerImpl::ReadUpdaterStateAttributes,
                     base::Unretained(this)),
      base::BindOnce(
          [](base::OnceCallback<void(const UpdaterStateAttributes&,
                                     const std::set<std::string>&)>
                 check_for_updates_invoker,
             PersistedData* metadata, std::vector<std::string> ids,
             const UpdaterStateAttributes& updater_state_attributes) {
            metadata->GetActiveBits(
                ids, base::BindOnce(std::move(check_for_updates_invoker),
                                    updater_state_attributes));
          },
          std::move(check_for_updates_invoker), base::Unretained(metadata_),
          context->components_to_check_for_updates));
}

// This function runs on the blocking pool task runner.
UpdaterStateAttributes UpdateCheckerImpl::ReadUpdaterStateAttributes() const {
#if BUILDFLAG(IS_WIN)
  // On Windows, the Chrome and the updater install modes are matched by design.
  return config_->GetUpdaterStateProvider().Run(!config_->IsPerUserInstall());
#elif BUILDFLAG(IS_MAC)
  return config_->GetUpdaterStateProvider().Run(false);
#else
  return {};
#endif  // BUILDFLAG(IS_WIN)
}

void UpdateCheckerImpl::CheckForUpdatesHelper(
    scoped_refptr<UpdateContext> context,
    const std::vector<GURL>& urls,
    const base::flat_map<std::string, std::string>& additional_attributes,
    const UpdaterStateAttributes& updater_state_attributes,
    const std::set<std::string>& active_ids) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (urls.empty()) {
    UpdateCheckFailed(ErrorCategory::kUpdateCheck,
                      static_cast<int>(ProtocolError::MISSING_URLS), 0);
    return;
  }
  GURL url = urls.front();

  // Components in this update check are either all foreground, or all
  // background since this member is inherited from the component's update
  // context. Pick the state of the first component to use in the update check.
  DCHECK(!context->components.empty());
  const bool is_foreground =
      context->components.cbegin()->second->is_foreground();
  DCHECK(base::ranges::all_of(
      context->components,
      [is_foreground](IdToComponentPtrMap::const_reference& elem) {
        return is_foreground == elem.second->is_foreground();
      }));

  std::vector<std::string> sent_ids;

  std::vector<protocol_request::App> apps;
  for (const auto& app_id : context->components_to_check_for_updates) {
    DCHECK_EQ(1u, context->components.count(app_id));
    const auto& component = context->components.at(app_id);
    DCHECK_EQ(component->id(), app_id);
    const auto& crx_component = component->crx_component();
    DCHECK(crx_component);

    if (crx_component->requires_network_encryption &&
        !url.SchemeIsCryptographic()) {
      continue;
    }

    sent_ids.push_back(app_id);

    std::string install_source;
    if (!crx_component->install_source.empty())
      install_source = crx_component->install_source;
    else if (component->is_foreground())
      install_source = "ondemand";

    apps.push_back(MakeProtocolApp(
        app_id, crx_component->version, crx_component->ap, crx_component->brand,
        config_->GetLang(), metadata_->GetInstallDate(app_id), install_source,
        crx_component->install_location, crx_component->fingerprint,
        crx_component->installer_attributes, metadata_->GetCohort(app_id),
        metadata_->GetCohortHint(app_id), metadata_->GetCohortName(app_id),
        crx_component->channel, crx_component->disabled_reasons,
        MakeProtocolUpdateCheck(!crx_component->updates_enabled,
                                crx_component->target_version_prefix,
                                crx_component->rollback_allowed,
                                crx_component->same_version_update_allowed),
        [](const std::string& install_data_index)
            -> std::vector<protocol_request::Data> {
          if (install_data_index.empty())
            return {};
          else
            return {{"install", install_data_index, ""}};
        }(crx_component->install_data_index),
        MakeProtocolPing(app_id, metadata_,
                         active_ids.find(app_id) != active_ids.end()),
        absl::nullopt));
  }

  if (sent_ids.empty()) {
    // No apps could be checked over this URL.
    UpdateCheckFailed(ErrorCategory::kUpdateCheck,
                      static_cast<int>(ProtocolError::MISSING_URLS), 0);
    return;
  }

  const auto request = MakeProtocolRequest(
      !config_->IsPerUserInstall(), context->session_id, config_->GetProdId(),
      config_->GetBrowserVersion().GetString(), config_->GetChannel(),
      config_->GetOSLongName(), config_->GetDownloadPreference(),
      config_->IsMachineExternallyManaged(), additional_attributes,
      updater_state_attributes, std::move(apps));

  request_sender_ = std::make_unique<RequestSender>(config_);
  request_sender_->Send(
      {url},
      BuildUpdateCheckExtraRequestHeaders(config_->GetProdId(),
                                          config_->GetBrowserVersion(),
                                          sent_ids, is_foreground),
      config_->GetProtocolHandlerFactory()->CreateSerializer()->Serialize(
          request),
      config_->EnabledCupSigning(),
      base::BindOnce(&UpdateCheckerImpl::OnRequestSenderComplete,
                     base::Unretained(this), context,
                     urls.size() > 1
                         ? absl::optional<base::OnceClosure>(base::BindOnce(
                               &UpdateCheckerImpl::CheckForUpdatesHelper,
                               base::Unretained(this), context,
                               std::vector<GURL>(urls.begin() + 1, urls.end()),
                               additional_attributes, updater_state_attributes,
                               active_ids))
                         : absl::nullopt));
}

void UpdateCheckerImpl::OnRequestSenderComplete(
    scoped_refptr<UpdateContext> context,
    absl::optional<base::OnceClosure> fallback,
    int error,
    const std::string& response,
    int retry_after_sec) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (error) {
    VLOG(1) << "RequestSender failed " << error;
    if (fallback && retry_after_sec <= 0) {
      std::move(*fallback).Run();
    } else {
      UpdateCheckFailed(ErrorCategory::kUpdateCheck, error, retry_after_sec);
    }
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
  UpdateCheckSucceeded(context, parser->results(), retry_after_sec);
}

void UpdateCheckerImpl::UpdateCheckSucceeded(
    scoped_refptr<UpdateContext> context,
    const ProtocolParser::Results& results,
    int retry_after_sec) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const int daynum = results.daystart_elapsed_days;
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

  base::OnceClosure reply =
      base::BindOnce(std::move(update_check_callback_),
                     absl::make_optional<ProtocolParser::Results>(results),
                     ErrorCategory::kNone, 0, retry_after_sec);

  if (daynum != ProtocolParser::kNoDaystart) {
    metadata_->SetDateLastData(context->components_to_check_for_updates, daynum,
                               std::move(reply));
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                              std::move(reply));
}

void UpdateCheckerImpl::UpdateCheckFailed(ErrorCategory error_category,
                                          int error,
                                          int retry_after_sec) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(0, error);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(update_check_callback_), absl::nullopt,
                     error_category, error, retry_after_sec));
}

}  // namespace

std::unique_ptr<UpdateChecker> UpdateChecker::Create(
    scoped_refptr<Configurator> config,
    PersistedData* persistent) {
  return std::make_unique<UpdateCheckerImpl>(config, persistent);
}

}  // namespace update_client
