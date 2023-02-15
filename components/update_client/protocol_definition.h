// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PROTOCOL_DEFINITION_H_
#define COMPONENTS_UPDATE_CLIENT_PROTOCOL_DEFINITION_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/update_client/activity_data_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace update_client {

// The protocol versions so far are:
// * Version 3.1: it changes how the run actions are serialized.
// * Version 3.0: it is the version implemented by the desktop updaters.
constexpr char kProtocolVersion[] = "3.1";

// Due to implementation constraints of the JSON parser and serializer,
// precision of integer numbers greater than 2^53 is lost.
constexpr int64_t kProtocolMaxInt = 1LL << 53;

namespace protocol_request {

struct HW {
  uint32_t physmemory = 0;  // Physical memory rounded down to the closest GB.
  bool sse = false;
  bool sse2 = false;
  bool sse3 = false;
  bool sse41 = false;
  bool sse42 = false;
  bool ssse3 = false;
  bool avx = false;
};

struct OS {
  OS();
  OS(const OS&) = delete;
  OS& operator=(const OS&) = delete;
  OS(OS&&);
  OS& operator=(OS&&);
  ~OS();

  std::string platform;
  std::string version;
  std::string service_pack;
  std::string arch;
};

struct Updater {
  Updater();
  Updater(const Updater&);
  ~Updater();

  std::string name;
  std::string version;
  bool is_machine = false;
  bool autoupdate_check_enabled = false;
  absl::optional<int> last_started;
  absl::optional<int> last_checked;
  int update_policy = 0;
};

struct UpdateCheck {
  UpdateCheck();
  ~UpdateCheck();

  bool is_update_disabled = false;
  std::string target_version_prefix;
  bool rollback_allowed = false;
  bool same_version_update_allowed = false;
};

// `data` element.
struct Data {
  Data();
  Data(const Data& other);
  Data& operator=(const Data& other);
  Data(const std::string& name,
       const std::string& install_data_index,
       const std::string& untrusted_data);
  ~Data();

  // `name` can be either "install" or "untrusted", corresponding to
  // `install_data_index` and `untrusted_data`.
  std::string name;
  std::string install_data_index;
  std::string untrusted_data;
};

// didrun element. The element is named "ping" for legacy reasons.
struct Ping {
  Ping();
  Ping(const Ping&);
  ~Ping();

  // Preferred user count metrics ("ad" and "rd").
  absl::optional<int> date_last_active;
  absl::optional<int> date_last_roll_call;

  // Legacy user count metrics ("a" and "r").
  absl::optional<int> days_since_last_active_ping;
  int days_since_last_roll_call = 0;

  std::string ping_freshness;
};

struct App {
  App();
  App(const App&) = delete;
  App& operator=(const App&) = delete;
  App(App&&);
  App& operator=(App&&);
  ~App();

  std::string app_id;
  std::string version;
  std::string ap;
  base::flat_map<std::string, std::string> installer_attributes;
  std::string lang;
  std::string brand_code;
  int install_date = kDateUnknown;
  std::string install_source;
  std::string install_location;
  std::string fingerprint;

  std::string cohort;       // Opaque string.
  std::string cohort_hint;  // Server may use to move the app to a new cohort.
  std::string cohort_name;  // Human-readable interpretation of the cohort.

  std::string release_channel;

  absl::optional<bool> enabled;
  absl::optional<std::vector<int>> disabled_reasons;

  // Optional update check.
  absl::optional<UpdateCheck> update_check;

  // Optional `data` elements.
  std::vector<Data> data;

  // Optional 'did run' ping.
  absl::optional<Ping> ping;

  // Progress/result pings.
  absl::optional<std::vector<base::Value::Dict>> events;
};

struct Request {
  Request();
  Request(const Request&) = delete;
  Request& operator=(const Request&) = delete;
  Request(Request&&);
  Request& operator=(Request&&);
  ~Request();

  std::string protocol_version;

  // True if the updater operates in the per-system configuration.
  bool is_machine = false;

  // Unique identifier for this session, used to correlate multiple requests
  // associated with a single update operation.
  std::string session_id;

  // Unique identifier for this request, used to associate the same request
  // received multiple times on the server.
  std::string request_id;
  std::string updatername;
  std::string updaterversion;
  std::string prodversion;
  std::string updaterchannel;
  std::string prodchannel;
  std::string operating_system;
  std::string arch;
  std::string nacl_arch;

#if BUILDFLAG(IS_WIN)
  bool is_wow64 = false;
#endif

  // Provides a hint for what download urls should be returned by server.
  // This data member is controlled by group policy settings.
  // The only group policy value supported so far is |cacheable|.
  std::string dlpref;

  // True if this machine is part of a managed enterprise domain.
  absl::optional<bool> domain_joined;

  base::flat_map<std::string, std::string> additional_attributes;

  HW hw;

  OS os;

  absl::optional<Updater> updater;

  std::vector<App> apps;
};

}  // namespace protocol_request

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PROTOCOL_DEFINITION_H_
