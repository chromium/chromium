// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PROTOCOL_DEFINITION_H_
#define COMPONENTS_UPDATE_CLIENT_PROTOCOL_DEFINITION_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/values.h"
#include "build/build_config.h"

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
};

struct OS {
  OS();
  OS(OS&&);
  ~OS();

  std::string platform;
  std::string version;
  std::string service_pack;
  std::string arch;

 private:
  DISALLOW_COPY_AND_ASSIGN(OS);
};

struct Updater {
  Updater();
  Updater(const Updater&);
  ~Updater();

  std::string name;
  std::string version;
  bool is_machine = false;
  bool autoupdate_check_enabled = false;
  base::Optional<int> last_started;
  base::Optional<int> last_checked;
  int update_policy = 0;
};

struct UpdateCheck {
  UpdateCheck();
  ~UpdateCheck();

  bool is_update_disabled = false;
};

// didrun element. The element is named "ping" for legacy reasons.
struct Ping {
  Ping();
  Ping(const Ping&);
  ~Ping();

  // Preferred user count metrics ("ad" and "rd").
  base::Optional<int> date_last_active;
  base::Optional<int> date_last_roll_call;

  // Legacy user count metrics ("a" and "r").
  base::Optional<int> days_since_last_active_ping;
  int days_since_last_roll_call = 0;

  std::string ping_freshness;
};

struct App {
  App();
  App(App&&);
  ~App();

  std::string app_id;
  std::string version;
  base::flat_map<std::string, std::string> installer_attributes;
  std::string lang;
  std::string brand_code;
  std::string install_source;
  std::string install_location;
  std::string fingerprint;

  std::string cohort;       // Opaque string.
  std::string cohort_hint;  // Server may use to move the app to a new cohort.
  std::string cohort_name;  // Human-readable interpretation of the cohort.

  std::string release_channel;

  base::Optional<bool> enabled;
  base::Optional<std::vector<int>> disabled_reasons;

  // Optional update check.
  base::Optional<UpdateCheck> update_check;

  // Optional 'did run' ping.
  base::Optional<Ping> ping;

  // Progress/result pings.
  base::Optional<std::vector<base::Value>> events;

 private:
  DISALLOW_COPY_AND_ASSIGN(App);
};

struct Request {
  Request();
  Request(Request&&);
  ~Request();

  std::string protocol_version;

  // Unique identifier for this session, used to correlate multiple requests
  // associated with a single update operation.
  std::string session_id;

  // Unique identifier for this request, used to associate the same request
  // received multiple times on the server.
  std::string request_id;

  std::string updatername;
  std::string updaterversion;
  std::string prodversion;
  std::string lang;
  std::string updaterchannel;
  std::string prodchannel;
  std::string operating_system;
  std::string arch;
  std::string nacl_arch;

#if defined(OS_WIN)
  bool is_wow64 = false;
#endif

  // Provides a hint for what download urls should be returned by server.
  // This data member is controlled by group policy settings.
  // The only group policy value supported so far is |cacheable|.
  std::string dlpref;

  // True if this machine is part of a managed enterprise domain.
  base::Optional<bool> domain_joined;

  base::flat_map<std::string, std::string> additional_attributes;

  HW hw;

  OS os;

  base::Optional<Updater> updater;

  std::vector<App> apps;

 private:
  DISALLOW_COPY_AND_ASSIGN(Request);
};

}  // namespace protocol_request

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PROTOCOL_DEFINITION_H_
