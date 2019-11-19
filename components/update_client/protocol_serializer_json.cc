// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_serializer_json.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/update_client/updater_state.h"

namespace update_client {

using Value = base::Value;

std::string ProtocolSerializerJSON::Serialize(
    const protocol_request::Request& request) const {
  Value root_node(Value::Type::DICTIONARY);
  auto* request_node =
      root_node.SetKey("request", Value(Value::Type::DICTIONARY));
  request_node->SetKey("protocol", Value(request.protocol_version));
  request_node->SetKey("dedup", Value("cr"));
  request_node->SetKey("acceptformat", Value("crx2,crx3"));
  if (!request.additional_attributes.empty()) {
    for (const auto& attr : request.additional_attributes)
      request_node->SetKey(attr.first, Value(attr.second));
  }
  request_node->SetKey("sessionid", Value(request.session_id));
  request_node->SetKey("requestid", Value(request.request_id));
  request_node->SetKey("@updater", Value(request.updatername));
  request_node->SetKey("prodversion", Value(request.updaterversion));
  request_node->SetKey("updaterversion", Value(request.prodversion));
  request_node->SetKey("lang", Value(request.lang));
  request_node->SetKey("@os", Value(request.operating_system));
  request_node->SetKey("arch", Value(request.arch));
  request_node->SetKey("nacl_arch", Value(request.nacl_arch));
#if defined(OS_WIN)
  if (request.is_wow64)
    request_node->SetKey("wow64", Value(request.is_wow64));
#endif  // OS_WIN
  if (!request.updaterchannel.empty())
    request_node->SetKey("updaterchannel", Value(request.updaterchannel));
  if (!request.prodchannel.empty())
    request_node->SetKey("prodchannel", Value(request.prodchannel));
  if (!request.dlpref.empty())
    request_node->SetKey("dlpref", Value(request.dlpref));
  if (request.domain_joined) {
    request_node->SetKey(UpdaterState::kIsEnterpriseManaged,
                         Value(*request.domain_joined));
  }

  // HW platform information.
  auto* hw_node = request_node->SetKey("hw", Value(Value::Type::DICTIONARY));
  hw_node->SetKey("physmemory", Value(static_cast<int>(request.hw.physmemory)));

  // OS version and platform information.
  auto* os_node = request_node->SetKey("os", Value(Value::Type::DICTIONARY));
  os_node->SetKey("platform", Value(request.os.platform));
  os_node->SetKey("arch", Value(request.os.arch));
  if (!request.os.version.empty())
    os_node->SetKey("version", Value(request.os.version));
  if (!request.os.service_pack.empty())
    os_node->SetKey("sp", Value(request.os.service_pack));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (request.updater) {
    const auto& updater = *request.updater;
    auto* updater_node =
        request_node->SetKey("updater", Value(Value::Type::DICTIONARY));
    updater_node->SetKey("name", Value(updater.name));
    updater_node->SetKey("ismachine", Value(updater.is_machine));
    updater_node->SetKey("autoupdatecheckenabled",
                         Value(updater.autoupdate_check_enabled));
    updater_node->SetKey("updatepolicy", Value(updater.update_policy));
    if (!updater.version.empty())
      updater_node->SetKey("version", Value(updater.version));
    if (updater.last_checked)
      updater_node->SetKey("lastchecked", Value(*updater.last_checked));
    if (updater.last_started)
      updater_node->SetKey("laststarted", Value(*updater.last_started));
  }
#endif

  std::vector<Value> app_nodes;
  for (const auto& app : request.apps) {
    Value app_node(Value::Type::DICTIONARY);
    app_node.SetKey("appid", Value(app.app_id));
    app_node.SetKey("version", Value(app.version));
    if (!app.brand_code.empty())
      app_node.SetKey("brand", Value(app.brand_code));
    if (!app.install_source.empty())
      app_node.SetKey("installsource", Value(app.install_source));
    if (!app.install_location.empty())
      app_node.SetKey("installedby", Value(app.install_location));
    if (!app.cohort.empty())
      app_node.SetKey("cohort", Value(app.cohort));
    if (!app.cohort_name.empty())
      app_node.SetKey("cohortname", Value(app.cohort_name));
    if (!app.cohort_hint.empty())
      app_node.SetKey("cohorthint", Value(app.cohort_hint));
    if (app.enabled)
      app_node.SetKey("enabled", Value(*app.enabled));

    if (app.disabled_reasons && !app.disabled_reasons->empty()) {
      std::vector<Value> disabled_nodes;
      for (const int disabled_reason : *app.disabled_reasons) {
        Value disabled_node(Value::Type::DICTIONARY);
        disabled_node.SetKey("reason", Value(disabled_reason));
        disabled_nodes.push_back(std::move(disabled_node));
      }
      app_node.SetKey("disabled", Value(disabled_nodes));
    }

    for (const auto& attr : app.installer_attributes)
      app_node.SetKey(attr.first, Value(attr.second));

    if (app.update_check) {
      auto* update_check_node =
          app_node.SetKey("updatecheck", Value(Value::Type::DICTIONARY));
      if (app.update_check->is_update_disabled)
        update_check_node->SetKey("updatedisabled", Value(true));
    }

    if (app.ping) {
      const auto& ping = *app.ping;
      auto* ping_node = app_node.SetKey("ping", Value(Value::Type::DICTIONARY));
      if (!ping.ping_freshness.empty())
        ping_node->SetKey("ping_freshness", Value(ping.ping_freshness));

      // Output "ad" or "a" only if the this app has been seen 'active'.
      if (ping.date_last_active) {
        ping_node->SetKey("ad", Value(*ping.date_last_active));
      } else if (ping.days_since_last_active_ping) {
        ping_node->SetKey("a", Value(*ping.days_since_last_active_ping));
      }

      // Output "rd" if valid or "r" as a last resort roll call metric.
      if (ping.date_last_roll_call)
        ping_node->SetKey("rd", Value(*ping.date_last_roll_call));
      else
        ping_node->SetKey("r", Value(ping.days_since_last_roll_call));
    }

    if (!app.fingerprint.empty()) {
      std::vector<Value> package_nodes;
      Value package(Value::Type::DICTIONARY);
      package.SetKey("fp", Value(app.fingerprint));
      package_nodes.push_back(std::move(package));
      auto* packages_node =
          app_node.SetKey("packages", Value(Value::Type::DICTIONARY));
      packages_node->SetKey("package", Value(package_nodes));
    }

    if (app.events) {
      std::vector<Value> event_nodes;
      for (const auto& event : *app.events) {
        DCHECK(event.is_dict());
        DCHECK(!event.DictEmpty());
        event_nodes.push_back(event.Clone());
      }
      app_node.SetKey("event", Value(event_nodes));
    }

    app_nodes.push_back(std::move(app_node));
  }

  if (!app_nodes.empty())
    request_node->SetKey("app", Value(std::move(app_nodes)));

  std::string msg;
  return base::JSONWriter::WriteWithOptions(
             root_node, base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION,
             &msg)
             ? msg
             : std::string();
}

}  // namespace update_client
