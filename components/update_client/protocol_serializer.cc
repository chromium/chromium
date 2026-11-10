// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_serializer.h"

#include <stddef.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/byte_size.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
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
#include "third_party/abseil-cpp/absl/strings/str_format.h"

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
  return base::ClampRound(
      base::SysInfo::AmountOfTotalPhysicalMemory().InGiBF());
}

std::string GetOSVersion() {
#if BUILDFLAG(IS_WIN)
  const auto ver = base::win::OSInfo::GetInstance()->version_number();
  return absl::StrFormat("%u.%u.%u.%u", ver.major, ver.minor, ver.build,
                         ver.patch);
#else
  return base::SysInfo().OperatingSystemVersion();
#endif
}

std::string GetServicePack() {
  return {};
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
  static constexpr size_t maxIdsCount = 30;
  const std::vector<std::string>& app_ids =
      ids.size() <= maxIdsCount
          ? ids
          : base::ToVector(base::span(ids).first(maxIdsCount));
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
    const std::string& device_form_factor,
    const std::string& download_preference,
    std::optional<bool> domain_joined,
    const base::flat_map<std::string, std::string>& additional_attributes,
    const base::flat_map<std::string, std::string>& updater_state_attributes,
    std::vector<protocol_request::App> apps) {
  CHECK(!session_id.empty());
  CHECK(session_id.starts_with('{'));
  CHECK(session_id.ends_with('}'));

  protocol_request::Request request = {
      .protocol_version = protocol_request::kProtocolVersion,
      .is_machine = is_machine,
      // Session id and request id.
      .session_id = session_id,
      .request_id = base::StrCat(
          {"{", base::Uuid::GenerateRandomV4().AsLowercaseString(), "}"}),
      .updatername = prod_id,
      .updaterversion = browser_version,
      .prodversion = browser_version,
      .updaterchannel = channel,
      .prodchannel = channel,
      .operating_system = std::string(UpdateQueryParams::GetOS()),
      .arch = std::string(UpdateQueryParams::GetArch()),
      .dlpref = download_preference,
      .domain_joined = domain_joined,
      .additional_attributes = additional_attributes,
      // OS version and platform information.
      .os = {.platform = os_long_name,
             .version = GetOSVersion(),
             .service_pack = GetServicePack(),
             .arch = GetArchitecture(),
             .device_form_factor = device_form_factor},
      .apps = std::move(apps)};

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

  return request;
}

protocol_request::App MakeProtocolApp(
    const std::string& app_id,
    const base::Version& version,
    const std::string& ap,
    const std::string& brand_code,
    const std::string& install_id,
    const std::string& lang,
    const int install_date,
    const std::string& install_source,
    const std::string& install_location,
    const std::map<std::string, std::string>& installer_attributes,
    const std::string& cohort,
    const std::string& cohort_hint,
    const std::string& cohort_name,
    const std::string& release_channel,
    const std::vector<int>& disabled_reasons,
    const std::vector<std::string>& cached_hashes,
    std::optional<protocol_request::UpdateCheck> update_check,
    const std::vector<protocol_request::Data>& data,
    std::optional<protocol_request::Ping> ping,
    std::optional<std::vector<base::DictValue>> events) {
  return {
      .app_id = app_id,
      .version = version.GetString(),
      .ap = ap,
      .installer_attributes = FilterInstallerAttributes(installer_attributes),
      .lang = lang,
      .brand_code = FilterBrandCode(brand_code),
      .install_date = install_date,
      .install_id = install_id,
      .install_source = install_source,
      .install_location = install_location,
      .cohort = cohort,
      .cohort_hint = cohort_hint,
      .cohort_name = cohort_name,
      .release_channel = release_channel,
      .enabled = disabled_reasons.empty(),
      .disabled_reasons = disabled_reasons,
      .update_check = std::move(update_check),
      .data = data,
      .ping = std::move(ping),
      .events = std::move(events),
      .cached_hashes = cached_hashes};
}

protocol_request::UpdateCheck MakeProtocolUpdateCheck(
    bool is_update_disabled,
    const std::string& target_version_prefix,
    bool rollback_allowed,
    bool same_version_update_allowed) {
  return {.is_update_disabled = is_update_disabled,
          .target_version_prefix = target_version_prefix,
          .rollback_allowed = rollback_allowed,
          .same_version_update_allowed = same_version_update_allowed};
}

protocol_request::Ping MakeProtocolPing(const std::string& app_id,
                                        const PersistedData* metadata,
                                        bool active) {
  CHECK(metadata);
  protocol_request::Ping ping = {.ping_freshness =
                                     metadata->GetPingFreshness(app_id)};

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

  return ping;
}

}  // namespace update_client
