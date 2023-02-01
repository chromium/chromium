// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/ping_manager.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace update_client {

namespace {

const int kErrorNoEvents = -1;
const int kErrorNoUrl = -2;

// An instance of this class can send only one ping.
class PingSender : public base::RefCountedThreadSafe<PingSender> {
 public:
  using Callback = PingManager::Callback;

  explicit PingSender(scoped_refptr<Configurator> config);

  PingSender(const PingSender&) = delete;
  PingSender& operator=(const PingSender&) = delete;

  void SendPing(const Component& component,
                const PersistedData& metadata,
                Callback callback);

 protected:
  virtual ~PingSender();

 private:
  friend class base::RefCountedThreadSafe<PingSender>;
  void SendPingComplete(int error,
                        const std::string& response,
                        int retry_after_sec);

  SEQUENCE_CHECKER(sequence_checker_);

  const scoped_refptr<Configurator> config_;
  Callback callback_;
  std::unique_ptr<RequestSender> request_sender_;
};

PingSender::PingSender(scoped_refptr<Configurator> config) : config_(config) {}

PingSender::~PingSender() = default;

void PingSender::SendPing(const Component& component,
                          const PersistedData& metadata,
                          Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (component.events().empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), kErrorNoEvents, ""));
    return;
  }

  DCHECK(component.crx_component());

  auto urls(config_->PingUrl());
  if (component.crx_component()->requires_network_encryption)
    RemoveUnsecureUrls(&urls);

  if (urls.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), kErrorNoUrl, ""));
    return;
  }

  callback_ = std::move(callback);

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
      absl::nullopt /* update check */, {} /* data */, absl::nullopt /* ping */,
      component.GetEvents()));
  request_sender_ = std::make_unique<RequestSender>(config_);
  request_sender_->Send(
      urls, {},
      config_->GetProtocolHandlerFactory()->CreateSerializer()->Serialize(
          MakeProtocolRequest(
              !config_->IsPerUserInstall(), component.session_id(),
              config_->GetProdId(), config_->GetBrowserVersion().GetString(),
              config_->GetChannel(), config_->GetOSLongName(),
              config_->GetDownloadPreference(),
              config_->IsMachineExternallyManaged(),
              config_->ExtraRequestParams(), {}, std::move(apps))),
      false, base::BindOnce(&PingSender::SendPingComplete, this));
}

void PingSender::SendPingComplete(int error,
                                  const std::string& response,
                                  int retry_after_sec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback_).Run(error, response);
}

}  // namespace

PingManager::PingManager(scoped_refptr<Configurator> config)
    : config_(config) {}

PingManager::~PingManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PingManager::SendPing(const Component& component,
                           const PersistedData& metadata,
                           Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto ping_sender = base::MakeRefCounted<PingSender>(config_);
  ping_sender->SendPing(component, metadata, std::move(callback));
}

}  // namespace update_client
