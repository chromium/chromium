// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_serializer_json.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/update_client/activity_data_service.h"

namespace update_client {

std::string ProtocolSerializerJSON::Serialize(
    const protocol_request::Request& request) const {
  base::Value::Dict root_node;
  base::Value::Dict request_node;
  request_node.Set("protocol", request.protocol_version);
  request_node.Set("ismachine", request.is_machine);
  request_node.Set("dedup", "cr");
  request_node.Set("acceptformat", "crx3,puff");
  if (!request.additional_attributes.empty()) {
    for (const auto& [name, value] : request.additional_attributes) {
      request_node.Set(name, value);
    }
  }
  request_node.Set("sessionid", request.session_id);
  request_node.Set("requestid", request.request_id);
  request_node.Set("@updater", request.updatername);
  request_node.Set("prodversion", request.prodversion);
  request_node.Set("updaterversion", request.updaterversion);
  request_node.Set("@os", request.operating_system);
  request_node.Set("arch", request.arch);
  request_node.Set("nacl_arch", request.nacl_arch);
#if BUILDFLAG(IS_WIN)
  if (request.is_wow64) {
    request_node.Set("wow64", request.is_wow64);
  }
#endif  // BUILDFLAG(IS_WIN)
  if (!request.updaterchannel.empty()) {
    request_node.Set("updaterchannel", request.updaterchannel);
  }
  if (!request.prodchannel.empty()) {
    request_node.Set("prodchannel", request.prodchannel);
  }
  if (!request.dlpref.empty()) {
    request_node.Set("dlpref", request.dlpref);
  }
  if (request.domain_joined) {
    request_node.Set("domainjoined", *request.domain_joined);
  }

  // HW platform information.
  base::Value::Dict hw_node;
  hw_node.Set("physmemory", static_cast<int>(request.hw.physmemory));
  hw_node.Set("sse", request.hw.sse);
  hw_node.Set("sse2", request.hw.sse2);
  hw_node.Set("sse3", request.hw.sse3);
  hw_node.Set("sse41", request.hw.sse41);
  hw_node.Set("sse42", request.hw.sse42);
  hw_node.Set("ssse3", request.hw.ssse3);
  hw_node.Set("avx", request.hw.avx);
  request_node.Set("hw", std::move(hw_node));

  // OS version and platform information.
  base::Value::Dict os_node;
  os_node.Set("platform", request.os.platform);
  os_node.Set("arch", request.os.arch);
  if (!request.os.version.empty()) {
    os_node.Set("version", request.os.version);
  }
  if (!request.os.service_pack.empty()) {
    os_node.Set("sp", request.os.service_pack);
  }
  request_node.Set("os", std::move(os_node));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (request.updater) {
    const auto& updater = *request.updater;
    base::Value::Dict updater_node;
    updater_node.Set("name", updater.name);
    updater_node.Set("ismachine", updater.is_machine);
    updater_node.Set("autoupdatecheckenabled",
                     updater.autoupdate_check_enabled);
    updater_node.Set("updatepolicy", updater.update_policy);
    if (!updater.version.empty()) {
      updater_node.Set("version", updater.version);
    }
    if (updater.last_checked) {
      updater_node.Set("lastchecked", *updater.last_checked);
    }
    if (updater.last_started) {
      updater_node.Set("laststarted", *updater.last_started);
    }
    request_node.Set("updater", std::move(updater_node));
  }
#endif

  base::Value::List app_nodes;
  for (const auto& app : request.apps) {
    base::Value::Dict app_node;
    app_node.Set("appid", app.app_id);
    app_node.Set("version", app.version);
    if (!app.ap.empty()) {
      app_node.Set("ap", app.ap);
    }
    if (!app.brand_code.empty()) {
      app_node.Set("brand", app.brand_code);
    }
    if (!app.lang.empty()) {
      app_node.Set("lang", app.lang);
    }
    if (app.install_date != kDateUnknown) {
      app_node.Set("installdate", app.install_date);
    }
    if (!app.install_source.empty()) {
      app_node.Set("installsource", app.install_source);
    }
    if (!app.install_location.empty()) {
      app_node.Set("installedby", app.install_location);
    }
    // TODO(crbug.com/40145897): Test that this is never sent to the server if
    // the machine is not enterprise managed.
    if (!app.release_channel.empty()) {
      app_node.Set("release_channel", app.release_channel);
    }
    if (!app.cohort.empty()) {
      app_node.Set("cohort", app.cohort);
    }
    if (!app.cohort_name.empty()) {
      app_node.Set("cohortname", app.cohort_name);
    }
    if (!app.cohort_hint.empty()) {
      app_node.Set("cohorthint", app.cohort_hint);
    }
    if (app.enabled) {
      app_node.Set("enabled", *app.enabled);
    }

    if (app.disabled_reasons && !app.disabled_reasons->empty()) {
      base::Value::List disabled_nodes;
      for (const int disabled_reason : *app.disabled_reasons) {
        base::Value::Dict disabled_node;
        disabled_node.Set("reason", disabled_reason);
        disabled_nodes.Append(std::move(disabled_node));
      }
      app_node.Set("disabled", std::move(disabled_nodes));
    }

    for (const auto& [name, value] : app.installer_attributes) {
      app_node.Set(name, value);
    }

    if (app.update_check) {
      base::Value::Dict update_check_node;
      if (app.update_check->is_update_disabled) {
        update_check_node.Set("updatedisabled", true);
      }
      if (app.update_check->rollback_allowed) {
        update_check_node.Set("rollback_allowed", true);
      }
      if (app.update_check->same_version_update_allowed) {
        update_check_node.Set("sameversionupdate", true);
      }
      if (!app.update_check->target_version_prefix.empty()) {
        update_check_node.Set("targetversionprefix",
                              app.update_check->target_version_prefix);
      }
      app_node.Set("updatecheck", std::move(update_check_node));
    }

    if (!app.data.empty()) {
      base::Value::List data_nodes;
      for (const auto& data : app.data) {
        base::Value::Dict data_node;

        data_node.Set("name", data.name);
        if (data.name == "install") {
          data_node.Set("index", data.install_data_index);
        } else if (data.name == "untrusted") {
          data_node.Set("#text", data.untrusted_data);
        }

        data_nodes.Append(std::move(data_node));
      }

      if (!data_nodes.empty()) {
        app_node.Set("data", std::move(data_nodes));
      }
    }

    if (app.ping) {
      const auto& ping = *app.ping;
      base::Value::Dict ping_node;
      if (!ping.ping_freshness.empty()) {
        ping_node.Set("ping_freshness", ping.ping_freshness);
      }

      // Output "ad" or "a" only if the this app has been seen 'active'.
      if (ping.date_last_active) {
        ping_node.Set("ad", *ping.date_last_active);
      } else if (ping.days_since_last_active_ping) {
        ping_node.Set("a", *ping.days_since_last_active_ping);
      }

      // Output "rd" if valid or "r" as a last resort roll call metric.
      if (ping.date_last_roll_call) {
        ping_node.Set("rd", *ping.date_last_roll_call);
      } else {
        ping_node.Set("r", ping.days_since_last_roll_call);
      }
      app_node.Set("ping", std::move(ping_node));
    }

    if (!app.fingerprint.empty()) {
      base::Value::List package_nodes;
      base::Value::Dict package;
      package.Set("fp", app.fingerprint);
      package_nodes.Append(std::move(package));
      base::Value::Dict packages_node;
      packages_node.Set("package", std::move(package_nodes));
      app_node.Set("packages", std::move(packages_node));
    }

    if (app.events) {
      base::Value::List event_nodes;
      for (const auto& event : *app.events) {
        CHECK(!event.empty());
        event_nodes.Append(event.Clone());
      }
      app_node.Set("event", std::move(event_nodes));
    }

    app_nodes.Append(std::move(app_node));
  }

  if (!app_nodes.empty()) {
    request_node.Set("app", std::move(app_nodes));
  }

  root_node.Set("request", std::move(request_node));

  std::string msg;
  return base::JSONWriter::WriteWithOptions(
             root_node, base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION,
             &msg)
             ? msg
             : std::string();
}

}  // namespace update_client
