// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/ping_manager.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "components/update_client/configurator.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/protocol_serializer.h"
#include "components/update_client/request_sender.h"
#include "components/update_client/update_client.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

namespace update_client {

PingManager::PingManager(scoped_refptr<Configurator> config)
    : config_(config) {}

PingManager::~PingManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PingManager::SendPing(const std::string& session_id,
                           const CrxComponent& component,
                           std::vector<base::Value::Dict> events,
                           base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (events.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback)));
    return;
  }

  auto urls(config_->PingUrl());
  if (component.requires_network_encryption) {
    RemoveUnsecureUrls(&urls);
  }

  if (urls.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback)));
    return;
  }

  const PersistedData& metadata = *config_->GetPersistedData();
  std::vector<protocol_request::App> apps;
  apps.push_back(MakeProtocolApp(
      component.app_id, component.version, component.ap, component.brand,
      config_->GetLang(), metadata.GetInstallDate(component.app_id),
      component.install_source, component.install_location,
      component.fingerprint, component.installer_attributes,
      metadata.GetCohort(component.app_id),
      metadata.GetCohortHint(component.app_id),
      metadata.GetCohortName(component.app_id), component.channel,
      component.disabled_reasons, std::nullopt /* update check */,
      {} /* data */, std::nullopt /* ping */, std::move(events)));
  base::MakeRefCounted<RequestSender>(config_->GetNetworkFetcherFactory())
      ->Send(
          urls, {},
          config_->GetProtocolHandlerFactory()->CreateSerializer()->Serialize(
              MakeProtocolRequest(
                  !config_->IsPerUserInstall(), session_id,
                  config_->GetProdId(),
                  config_->GetBrowserVersion().GetString(),
                  config_->GetChannel(), config_->GetOSLongName(),
                  config_->GetDownloadPreference(),
                  config_->IsMachineExternallyManaged(),
                  config_->ExtraRequestParams(), {}, std::move(apps))),
          false,
          base::BindOnce([](base::OnceClosure callback, int error,
                            const std::string& response,
                            int retry_after_sec) { std::move(callback).Run(); },
                         std::move(callback)));
}

}  // namespace update_client
