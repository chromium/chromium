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
#include "components/update_client/component.h"
#include "components/update_client/configurator.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/protocol_serializer.h"
#include "components/update_client/request_sender.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

namespace update_client {

PingManager::PingManager(scoped_refptr<Configurator> config)
    : config_(config) {}

PingManager::~PingManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PingManager::SendPing(const Component& component,
                           base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (component.events().empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback)));
    return;
  }

  CHECK(component.crx_component());

  auto urls(config_->PingUrl());
  if (component.crx_component()->requires_network_encryption) {
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
      component.id(), component.crx_component()->version,
      component.crx_component()->ap, component.crx_component()->brand,
      config_->GetLang(), metadata.GetInstallDate(component.id()),
      component.crx_component()->install_source,
      component.crx_component()->install_location,
      component.crx_component()->fingerprint,
      component.crx_component()->installer_attributes,
      metadata.GetCohort(component.id()),
      metadata.GetCohortHint(component.id()),
      metadata.GetCohortName(component.id()),
      component.crx_component()->channel,
      component.crx_component()->disabled_reasons,
      std::nullopt /* update check */, {} /* data */, std::nullopt /* ping */,
      component.GetEvents()));
  base::MakeRefCounted<RequestSender>(config_->GetNetworkFetcherFactory())
      ->Send(
          urls, {},
          config_->GetProtocolHandlerFactory()->CreateSerializer()->Serialize(
              MakeProtocolRequest(
                  !config_->IsPerUserInstall(), component.session_id(),
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
