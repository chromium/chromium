// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_serializer.h"

#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/uuid.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/update_query_params.h"
#include "components/update_client/utils.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(ARCH_CPU_X86_FAMILY)
#include "base/cpu.h"
#endif

namespace update_client {

namespace {

// Returns the amount of physical memory in GB, rounded to the nearest GB.
int GetPhysicalMemoryGB() {
  return base::ClampRound(base::SysInfo::AmountOfPhysicalMemoryMB() / 1024.0f);
}

std::string GetOSVersion() {
#if BUILDFLAG(IS_WIN)
  const auto ver = base::win::OSInfo::GetInstance()->version_number();
  return base::StringPrintf("%u.%u.%u.%u", ver.major, ver.minor, ver.build,
                            ver.patch);
#else
  return base::SysInfo().OperatingSystemVersion();
#endif
}

std::string GetServicePack() {
#if BUILDFLAG(IS_WIN)
  return base::win::OSInfo::GetInstance()->service_pack_str();
#else
  return {};
#endif
}

// Returns brand code in the expected format, or an empty string otherwise.
std::string FilterBrandCode(const std::string& brand) {
  return IsValidBrand(brand) ? brand : std::string("");
}

// Filters invalid attributes from |installer_attributes|.
base::flat_map<std::string, std::string> FilterInstallerAttributes(
    const InstallerAttributes& installer_attributes) {
  base::flat_map<std::string, std::string> sanitized_attrs;
  for (const auto& attr : installer_attributes) {
    if (IsValidInstallerAttribute(attr)) {
      sanitized_attrs.insert(attr);
    }
  }
  return sanitized_attrs;
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
    const bool is_machine,
    const std::string& session_id,
    const std::string& prod_id,
    const std::string& browser_version,
    const std::string& channel,
    const std::string& os_long_name,
    const std::string& download_preference,
    std::optional<bool> domain_joined,
    const base::flat_map<std::string, std::string>& additional_attributes,
    const base::flat_map<std::string, std::string>& updater_state_attributes,
    std::vector<protocol_request::App> apps) {
  protocol_request::Request request;
  request.protocol_version = protocol_request::kProtocolVersion;
  request.is_machine = is_machine;

  // Session id and request id.
  CHECK(!session_id.empty());
  CHECK(base::StartsWith(session_id, "{", base::CompareCase::SENSITIVE));
  CHECK(base::EndsWith(session_id, "}", base::CompareCase::SENSITIVE));
  request.session_id = session_id;
  request.request_id = base::StrCat(
      {"{", base::Uuid::GenerateRandomV4().AsLowercaseString(), "}"});

  request.updatername = prod_id;
  request.updaterversion = browser_version;
  request.prodversion = browser_version;
  request.updaterchannel = channel;
  request.prodchannel = channel;
  request.operating_system = UpdateQueryParams::GetOS();
  request.arch = UpdateQueryParams::GetArch();
  request.nacl_arch = UpdateQueryParams::GetNaclArch();
  request.dlpref = download_preference;
  request.domain_joined = domain_joined;
  request.additional_attributes = additional_attributes;

#if BUILDFLAG(IS_WIN)
  if (base::win::OSInfo::GetInstance()->IsWowX86OnAMD64()) {
    request.is_wow64 = true;
  }
#endif

  // HW platform information.
  request.hw.physmemory = GetPhysicalMemoryGB();
#if defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu;
  request.hw.sse = cpu.has_sse();
  request.hw.sse2 = cpu.has_sse2();
  request.hw.sse3 = cpu.has_sse3();
  request.hw.ssse3 = cpu.has_ssse3();
  request.hw.sse41 = cpu.has_sse41();
  request.hw.sse42 = cpu.has_sse42();
  request.hw.avx = cpu.has_avx();
#endif

  // OS version and platform information.
  request.os.platform = os_long_name;
  request.os.version = GetOSVersion();
  request.os.service_pack = GetServicePack();
  request.os.arch = GetArchitecture();

  if (!updater_state_attributes.empty()) {
    request.updater = std::make_optional<protocol_request::Updater>();
    auto it = updater_state_attributes.find("name");
    if (it != updater_state_attributes.end()) {
      request.updater->name = it->second;
    }
    it = updater_state_attributes.find("version");
    if (it != updater_state_attributes.end()) {
      request.updater->version = it->second;
    }
    it = updater_state_attributes.find("ismachine");
    if (it != updater_state_attributes.end()) {
      CHECK(it->second == "0" || it->second == "1");
      request.updater->is_machine = it->second != "0";
    }
    it = updater_state_attributes.find("autoupdatecheckenabled");
    if (it != updater_state_attributes.end()) {
      CHECK(it->second == "0" || it->second == "1");
      request.updater->autoupdate_check_enabled = it->second != "0";
    }
    it = updater_state_attributes.find("laststarted");
    if (it != updater_state_attributes.end()) {
      int last_started = 0;
      if (base::StringToInt(it->second, &last_started)) {
        request.updater->last_started = last_started;
      }
    }
    it = updater_state_attributes.find("lastchecked");
    if (it != updater_state_attributes.end()) {
      int last_checked = 0;
      if (base::StringToInt(it->second, &last_checked)) {
        request.updater->last_checked = last_checked;
      }
    }
    it = updater_state_attributes.find("updatepolicy");
    if (it != updater_state_attributes.end()) {
      int update_policy = 0;
      if (base::StringToInt(it->second, &update_policy)) {
        request.updater->update_policy = update_policy;
      }
    }
  }

  request.apps = std::move(apps);
  return request;
}

protocol_request::App MakeProtocolApp(
    const std::string& app_id,
    const base::Version& version,
    const std::string& ap,
    const std::string& brand_code,
    const std::string& lang,
    const int install_date,
    const std::string& install_source,
    const std::string& install_location,
    const std::string& fingerprint,
    const std::map<std::string, std::string>& installer_attributes,
    const std::string& cohort,
    const std::string& cohort_hint,
    const std::string& cohort_name,
    const std::string& release_channel,
    const std::vector<int>& disabled_reasons,
    std::optional<protocol_request::UpdateCheck> update_check,
    const std::vector<protocol_request::Data>& data,
    std::optional<protocol_request::Ping> ping,
    std::optional<std::vector<base::Value::Dict>> events) {
  protocol_request::App app;
  app.app_id = app_id;
  app.version = version.GetString();
  app.ap = ap;
  app.events = std::move(events);
  app.brand_code = FilterBrandCode(brand_code);
  app.lang = lang;
  app.install_date = install_date;
  app.install_source = install_source;
  app.install_location = install_location;
  app.fingerprint = fingerprint;
  app.installer_attributes = FilterInstallerAttributes(installer_attributes);
  app.cohort = cohort;
  app.cohort_hint = cohort_hint;
  app.cohort_name = cohort_name;
  app.release_channel = release_channel;
  app.enabled = disabled_reasons.empty();
  app.disabled_reasons = disabled_reasons;
  app.update_check = std::move(update_check);
  app.data = data;
  app.ping = std::move(ping);
  return app;
}

protocol_request::UpdateCheck MakeProtocolUpdateCheck(
    bool is_update_disabled,
    const std::string& target_version_prefix,
    bool rollback_allowed,
    bool same_version_update_allowed) {
  protocol_request::UpdateCheck update_check;
  update_check.is_update_disabled = is_update_disabled;
  update_check.target_version_prefix = target_version_prefix;
  update_check.rollback_allowed = rollback_allowed;
  update_check.same_version_update_allowed = same_version_update_allowed;
  return update_check;
}

protocol_request::Ping MakeProtocolPing(const std::string& app_id,
                                        const PersistedData* metadata,
                                        bool active) {
  CHECK(metadata);
  protocol_request::Ping ping;

  if (active) {
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
