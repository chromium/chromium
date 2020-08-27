// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_serializer.h"

#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/update_query_params.h"
#include "components/update_client/updater_state.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace update_client {

namespace {

// Returns the amount of physical memory in GB, rounded to the nearest GB.
int GetPhysicalMemoryGB() {
  const double kOneGB = 1024 * 1024 * 1024;
  const int64_t phys_mem = base::SysInfo::AmountOfPhysicalMemory();
  return static_cast<int>(std::floor(0.5 + phys_mem / kOneGB));
}

std::string GetOSVersion() {
#if defined(OS_WIN)
  const auto ver = base::win::OSInfo::GetInstance()->version_number();
  return base::StringPrintf("%d.%d.%d.%d", ver.major, ver.minor, ver.build,
                            ver.patch);
#else
  return base::SysInfo().OperatingSystemVersion();
#endif
}

std::string GetServicePack() {
#if defined(OS_WIN)
  return base::win::OSInfo::GetInstance()->service_pack_str();
#else
  return {};
#endif
}

}  // namespace

base::flat_map<std::string, std::string> BuildUpdateCheckExtraRequestHeaders(
    const std::string& prod_id,
    const base::Version& browser_version,
    const std::vector<std::string>& ids,
    bool is_foreground) {
  // This number of extension ids results in an HTTP header length of about 1KB.
  constexpr size_t maxIdsCount = 30;
  const std::vector<std::string>& app_ids =
      ids.size() <= maxIdsCount
          ? ids
          : std::vector<std::string>(ids.cbegin(), ids.cbegin() + maxIdsCount);
  return {
      {"X-Goog-Update-Updater",
       base::StrCat({prod_id, "-", browser_version.GetString()})},
      {"X-Goog-Update-Interactivity", is_foreground ? "fg" : "bg"},
      {"X-Goog-Update-AppId", base::JoinString(app_ids, ",")},
  };
}

protocol_request::Request MakeProtocolRequest(
    const std::string& session_id,
    const std::string& prod_id,
    const std::string& browser_version,
    const std::string& lang,
    const std::string& channel,
    const std::string& os_long_name,
    const std::string& download_preference,
    const base::flat_map<std::string, std::string>& additional_attributes,
    const std::map<std::string, std::string>* updater_state_attributes,
    std::vector<protocol_request::App> apps) {
  protocol_request::Request request;
  request.protocol_version = kProtocolVersion;

  // Session id and request id.
  DCHECK(!session_id.empty());
  DCHECK(base::StartsWith(session_id, "{", base::CompareCase::SENSITIVE));
  DCHECK(base::EndsWith(session_id, "}", base::CompareCase::SENSITIVE));
  request.session_id = session_id;
  request.request_id = base::StrCat({"{", base::GenerateGUID(), "}"});

  request.updatername = prod_id;
  request.updaterversion = browser_version;
  request.prodversion = browser_version;
  request.lang = lang;
  request.updaterchannel = channel;
  request.prodchannel = channel;
  request.operating_system = UpdateQueryParams::GetOS();
  request.arch = UpdateQueryParams::GetArch();
  request.nacl_arch = UpdateQueryParams::GetNaclArch();
  request.dlpref = download_preference;
  request.additional_attributes = additional_attributes;

#if defined(OS_WIN)
  if (base::win::OSInfo::GetInstance()->wow64_status() ==
      base::win::OSInfo::WOW64_ENABLED)
    request.is_wow64 = true;
#endif

  if (updater_state_attributes &&
      updater_state_attributes->count(UpdaterState::kIsEnterpriseManaged)) {
    request.domain_joined =
        updater_state_attributes->at(UpdaterState::kIsEnterpriseManaged) == "1";
  }

  // HW platform information.
  request.hw.physmemory = GetPhysicalMemoryGB();

  // OS version and platform information.
  request.os.platform = os_long_name;
  request.os.version = GetOSVersion();
  request.os.service_pack = GetServicePack();
  request.os.arch = base::SysInfo().OperatingSystemArchitecture();

  if (updater_state_attributes) {
    request.updater = base::make_optional<protocol_request::Updater>();
    auto it = updater_state_attributes->find("name");
    if (it != updater_state_attributes->end())
      request.updater->name = it->second;
    it = updater_state_attributes->find("version");
    if (it != updater_state_attributes->end())
      request.updater->version = it->second;
    it = updater_state_attributes->find("ismachine");
    if (it != updater_state_attributes->end()) {
      DCHECK(it->second == "0" || it->second == "1");
      request.updater->is_machine = it->second != "0";
    }
    it = updater_state_attributes->find("autoupdatecheckenabled");
    if (it != updater_state_attributes->end()) {
      DCHECK(it->second == "0" || it->second == "1");
      request.updater->autoupdate_check_enabled = it->second != "0";
    }
    it = updater_state_attributes->find("laststarted");
    if (it != updater_state_attributes->end()) {
      int last_started = 0;
      if (base::StringToInt(it->second, &last_started))
        request.updater->last_started = last_started;
    }
    it = updater_state_attributes->find("lastchecked");
    if (it != updater_state_attributes->end()) {
      int last_checked = 0;
      if (base::StringToInt(it->second, &last_checked))
        request.updater->last_checked = last_checked;
    }
    it = updater_state_attributes->find("updatepolicy");
    if (it != updater_state_attributes->end()) {
      int update_policy = 0;
      if (base::StringToInt(it->second, &update_policy))
        request.updater->update_policy = update_policy;
    }
  }

  request.apps = std::move(apps);
  return request;
}

protocol_request::App MakeProtocolApp(
    const std::string& app_id,
    const base::Version& version,
    base::Optional<std::vector<base::Value>> events) {
  protocol_request::App app;
  app.app_id = app_id;
  app.version = version.GetString();
  app.events = std::move(events);
  return app;
}

protocol_request::App MakeProtocolApp(
    const std::string& app_id,
    const base::Version& version,
    const std::string& brand_code,
    const std::string& install_source,
    const std::string& install_location,
    const std::string& fingerprint,
    const base::flat_map<std::string, std::string>& installer_attributes,
    const std::string& cohort,
    const std::string& cohort_hint,
    const std::string& cohort_name,
    const std::string& release_channel,
    const std::vector<int>& disabled_reasons,
    base::Optional<protocol_request::UpdateCheck> update_check,
    base::Optional<protocol_request::Ping> ping) {
  auto app = MakeProtocolApp(app_id, version, base::nullopt);
  app.brand_code = brand_code;
  app.install_source = install_source;
  app.install_location = install_location;
  app.fingerprint = fingerprint;
  app.installer_attributes = installer_attributes;
  app.cohort = cohort;
  app.cohort_hint = cohort_hint;
  app.cohort_name = cohort_name;
  app.release_channel = release_channel;
  app.enabled = disabled_reasons.empty();
  app.disabled_reasons = disabled_reasons;
  app.update_check = std::move(update_check);
  app.ping = std::move(ping);
  return app;
}

protocol_request::UpdateCheck MakeProtocolUpdateCheck(bool is_update_disabled) {
  protocol_request::UpdateCheck update_check;
  update_check.is_update_disabled = is_update_disabled;
  return update_check;
}

protocol_request::Ping MakeProtocolPing(const std::string& app_id,
                                        const PersistedData* metadata) {
  DCHECK(metadata);
  protocol_request::Ping ping;

  if (metadata->GetActiveBit(app_id)) {
    const int date_last_active = metadata->GetDateLastActive(app_id);
    if (date_last_active != kDateUnknown) {
      ping.date_last_active = date_last_active;
    } else {
      ping.days_since_last_active_ping =
          metadata->GetDaysSinceLastActive(app_id);
    }
  }
  const int date_last_roll_call = metadata->GetDateLastRollCall(app_id);
  if (date_last_roll_call != kDateUnknown) {
    ping.date_last_roll_call = date_last_roll_call;
  } else {
    ping.days_since_last_roll_call = metadata->GetDaysSinceLastRollCall(app_id);
  }
  ping.ping_freshness = metadata->GetPingFreshness(app_id);

  return ping;
}

}  // namespace update_client
