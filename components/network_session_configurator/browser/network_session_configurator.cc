// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_session_configurator/browser/network_session_configurator.h"

#include <limits>
#include <map>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/network_session_configurator/common/network_features.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/variations/variations_switches.h"
#include "net/base/features.h"
#include "net/base/host_mapping_rules.h"
#include "net/disk_cache/backend_experiment.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_factory.h"
#include "net/quic/quic_context.h"
#include "net/quic/set_quic_flag.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/third_party/quiche/src/quiche/common/platform/api/quiche_flags.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_tag.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

// Map from name to value for all parameters associate with a field trial.
using VariationParameters = std::map<std::string, std::string>;

const char kQuicFieldTrialName[] = "QUIC";

const char kHttp2FieldTrialName[] = "HTTP2";

// Gets the value of the specified command line switch, as an integer. If unable
// to convert it to an int (It's not an int, or the switch is not present)
// returns 0.
int GetSwitchValueAsInt(const base::CommandLine& command_line,
                        const std::string& switch_name) {
  int value;
  if (!base::StringToInt(command_line.GetSwitchValueASCII(switch_name),
                         &value)) {
    return 0;
  }
  return value;
}

// Returns the value associated with |key| in |params| or "" if the
// key is not present in the map.
const std::string& GetVariationParam(
    const std::map<std::string, std::string>& params,
    const std::string& key) {
  auto it = params.find(key);
  if (it == params.end())
    return base::EmptyString();

  return it->second;
}

bool GetVariationBoolParamOrFeatureSetting(const VariationParameters& params,
                                           const std::string& key,
                                           bool feature_setting) {
  // Don't override feature setting if variation param doesn't exist.
  if (params.find(key) == params.end()) {
    return feature_setting;
  }

  return base::EqualsCaseInsensitiveASCII(GetVariationParam(params, key),
                                          "true");
}

spdy::SettingsMap GetHttp2Settings(
    const VariationParameters& http2_trial_params) {
  spdy::SettingsMap http2_settings;

  const std::string settings_string =
      GetVariationParam(http2_trial_params, "http2_settings");

  base::StringPairs key_value_pairs;
  if (!base::SplitStringIntoKeyValuePairs(settings_string, ':', ',',
                                          &key_value_pairs)) {
    return http2_settings;
  }

  for (auto key_value : key_value_pairs) {
    uint32_t key;
    if (!base::StringToUint(key_value.first, &key))
      continue;
    uint32_t value;
    if (!base::StringToUint(key_value.second, &value))
      continue;
    http2_settings[static_cast<spdy::SpdyKnownSettingsId>(key)] = value;
  }

  return http2_settings;
}

int ConfigureSpdySessionMaxQueuedCappedFrames(
    const base::CommandLine& /*command_line*/,
    const VariationParameters& http2_trial_params) {
  int value;
  if (base::StringToInt(
          GetVariationParam(http2_trial_params,
                            "spdy_session_max_queued_capped_frames"),
          &value)) {
    return value;
  }
  return net::kSpdySessionMaxQueuedCappedFrames;
}

void ConfigureHttp2Params(const base::CommandLine& command_line,
                          std::string_view http2_trial_group,
                          const VariationParameters& http2_trial_params,
                          net::HttpNetworkSessionParams* params) {
  if (GetVariationParam(http2_trial_params, "http2_enabled") == "false") {
    params->enable_http2 = false;
    return;
  }

  params->http2_settings = GetHttp2Settings(http2_trial_params);

  // Enable/disable greasing SETTINGS, see
  // https://tools.ietf.org/html/draft-bishop-httpbis-grease-00.
  if (command_line.HasSwitch(switches::kDisableHttp2GreaseSettings)) {
    params->enable_http2_settings_grease = false;
  } else if (command_line.HasSwitch(switches::kEnableHttp2GreaseSettings) ||
             GetVariationParam(http2_trial_params, "http2_grease_settings") ==
                 "true") {
    params->enable_http2_settings_grease = true;
  }

  // Optionally define a frame of reserved type to "grease" frame types, see
  // https://tools.ietf.org/html/draft-bishop-httpbis-grease-00.
  if (command_line.HasSwitch(switches::kHttp2GreaseFrameType) ||
      GetVariationParam(http2_trial_params, "http2_grease_frame_type") ==
          "true") {
    const uint8_t type = 0x0b + 0x1f * base::RandGenerator(8);

    uint8_t flags;
    base::RandBytes(base::byte_span_from_ref(flags));

    const size_t length = base::RandGenerator(7);
    // RandBytesAsString() does not support zero length.
    const std::string payload =
        (length > 0) ? base::RandBytesAsString(length) : std::string();

    params->greased_http2_frame =
        std::optional<net::SpdySessionPool::GreasedHttp2Frame>(
            {type, flags, payload});
  }

  if (command_line.HasSwitch(switches::kHttp2EndStreamWithDataFrame) ||
      GetVariationParam(http2_trial_params,
                        "http2_end_stream_with_data_frame") == "true") {
    params->http2_end_stream_with_data_frame = true;
  }

  params->spdy_session_max_queued_capped_frames =
      ConfigureSpdySessionMaxQueuedCappedFrames(command_line,
                                                http2_trial_params);
}

bool ShouldDisableQuic(std::string_view quic_trial_group,
                       const VariationParameters& quic_trial_params,
                       bool is_quic_force_disabled) {
  if (is_quic_force_disabled)
    return true;

  return base::EqualsCaseInsensitiveASCII(
      GetVariationParam(quic_trial_params, "enable_quic"), "false");
}

bool ShouldRetryWithoutAltSvcOnQuicErrors(
    const VariationParameters& quic_trial_params) {
  return !base::EqualsCaseInsensitiveASCII(
      GetVariationParam(quic_trial_params,
                        "retry_without_alt_svc_on_quic_errors"),
      "false");
}

quic::QuicTagVector GetQuicConnectionOptions(
    const VariationParameters& quic_trial_params) {
  auto it = quic_trial_params.find("connection_options");
  if (it == quic_trial_params.end()) {
    return quic::QuicTagVector();
  }

  return quic::ParseQuicTagVector(it->second);
}

quic::QuicTagVector GetQuicClientConnectionOptions(
    const VariationParameters& quic_trial_params) {
  auto it = quic_trial_params.find("client_connection_options");
  if (it == quic_trial_params.end()) {
    return quic::QuicTagVector();
  }

  return quic::ParseQuicTagVector(it->second);
}

bool ShouldQuicCloseSessionsOnIpChange(
    const VariationParameters& quic_trial_params) {
  return base::EqualsCaseInsensitiveASCII(
      GetVariationParam(quic_trial_params, "close_sessions_on_ip_change"),
      "true");
}

bool ShouldQuicGoAwaySessionsOnIpChange(
    const VariationParameters& quic_trial_params) {
  return base::EqualsCaseInsensitiveASCII(
      GetVariationParam(quic_trial_params, "goaway_sessions_on_ip_change"),
      "true");
}

std::optional<bool> GetExponentialBackOffOnInitialDelay(
    const VariationParameters& quic_trial_params) {
  if (base::EqualsCaseInsensitiveASCII(
          GetVariationParam(quic_trial_params,
                            "exponential_backoff_on_initial_delay"),
          "false")) {
    return false;
  }
  if (base::EqualsCaseInsensitiveASCII(
          GetVariationParam(quic_trial_params,
                            "exponential_backoff_on_initial_delay"),
          "true")) {
    return true;
  }
  return std::nullopt;
}

int GetQuicIdleConnectionTimeoutSeconds(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(GetVariationParam(quic_trial_params,
                                          "idle_connection_timeout_seconds"),
                        &value)) {
    return value;
  }
  return 0;
}

int GetQuicReducedPingTimeoutSeconds(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(
          GetVariationParam(quic_trial_params, "reduced_ping_timeout_seconds"),
          &value)) {
    return value;
  }
  return 0;
}

int GetQuicMaxTimeBeforeCryptoHandshakeSeconds(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(
          GetVariationParam(quic_trial_params,
                            "max_time_before_crypto_handshake_seconds"),
          &value)) {
    return value;
  }
  return 0;
}

int GetQuicMaxIdleTimeBeforeCryptoHandshakeSeconds(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(
          GetVariationParam(quic_trial_params,
                            "max_idle_time_before_crypto_handshake_seconds"),
          &value)) {
    return value;
  }
  return 0;
}

bool ShouldQuicEstimateInitialRtt(
    const VariationParameters& quic_trial_params) {
  return base::EqualsCaseInsensitiveASCII(
      GetVariationParam(quic_trial_params, "estimate_initial_rtt"), "true");
}

bool ShouldQuicMigrateSessionsOnNetworkChangeV2(
    const VariationParameters& quic_trial_params) {
  return GetVariationBoolParamOrFeatureSetting(
      quic_trial_params, "migrate_sessions_on_network_change_v2",
      base::FeatureList::IsEnabled(
          net::features::kMigrateSessionsOnNetworkChangeV2));
}

bool ShouldQuicUseNewAlpsCodepoint(
    const VariationParameters& quic_trial_params) {
  return GetVariationBoolParamOrFeatureSetting(
      quic_trial_params, "use_new_alps_codepoint",
      base::FeatureList::IsEnabled(net::features::kUseNewAlpsCodepointQUIC));
}

bool ShouldQuicReportEcn(
    const VariationParameters& quic_trial_params) {
  return GetVariationBoolParamOrFeatureSetting(
      quic_trial_params, "report_ecn",
      base::FeatureList::IsEnabled(net::features::kReportEcn));
}

bool ShouldQuicMigrateSessionsEarlyV2(
    const VariationParameters& quic_trial_params) {
  return base::EqualsCaseInsensitiveASCII(
      GetVariationParam(quic_trial_params, "migrate_sessions_early_v2"),
      "true");
}

bool ShouldQuicAllowPortMigration(
    const VariationParameters& quic_trial_params) {
  return !base::EqualsCaseInsensitiveASCII(
      GetVariationParam(quic_trial_params, "allow_port_migration"), "false");
}

int GetMultiPortProbingInterval(const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(
          GetVariationParam(quic_trial_params, "multi_port_probing_interval"),
          &value)) {
    return value;
  }
  return 0;
}

bool ShouldQuicRetryOnAlternateNetworkBeforeHandshake(
    const VariationParameters& quic_trial_params) {
  return base::EqualsCaseInsensitiveASCII(
      GetVariationParam(quic_trial_params,
                        "retry_on_alternate_network_before_handshake"),
      "true");
}

int GetQuicMaxTimeOnNonDefaultNetworkSeconds(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(
          GetVariationParam(quic_trial_params,
                            "max_time_on_non_default_network_seconds"),
          &value)) {
    return value;
  }
  return 0;
}

bool ShouldQuicMigrateIdleSessions(
    const VariationParameters& quic_trial_params) {
  return base::EqualsCaseInsensitiveASCII(
      GetVariationParam(quic_trial_params, "migrate_idle_sessions"), "true");
}

bool ShouldQuicDisableTlsZeroRtt(const VariationParameters& quic_trial_params) {
  return base::EqualsCaseInsensitiveASCII(
      GetVariationParam(quic_trial_params, "disable_tls_zero_rtt"), "true");
}

bool ShouldQuicDisableGQuicZeroRtt(
    const VariationParameters& quic_trial_params) {
  return base::EqualsCaseInsensitiveASCII(
      GetVariationParam(quic_trial_params, "disable_gquic_zero_rtt"), "true");
}

int GetQuicRetransmittableOnWireTimeoutMilliseconds(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(
          GetVariationParam(quic_trial_params,
                            "retransmittable_on_wire_timeout_milliseconds"),
          &value)) {
    return value;
  }
  return 0;
}

int GetQuicIdleSessionMigrationPeriodSeconds(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(
          GetVariationParam(quic_trial_params,
                            "idle_session_migration_period_seconds"),
          &value)) {
    return value;
  }
  return 0;
}

int GetQuicMaxNumMigrationsToNonDefaultNetworkOnWriteError(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(
          GetVariationParam(
              quic_trial_params,
              "max_migrations_to_non_default_network_on_write_error"),
          &value)) {
    return value;
  }
  return 0;
}

int GetQuicMaxNumMigrationsToNonDefaultNetworkOnPathDegrading(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(
          GetVariationParam(
              quic_trial_params,
              "max_migrations_to_non_default_network_on_path_degrading"),
          &value)) {
    return value;
  }
  return 0;
}

int GetQuicInitialRttForHandshakeMilliseconds(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(
          GetVariationParam(quic_trial_params,
                            "initial_rtt_for_handshake_milliseconds"),
          &value)) {
    return value;
  }
  return 0;
}

base::flat_set<std::string> GetQuicHostAllowlist(
    const VariationParameters& quic_trial_params) {
  std::string host_allowlist =
      GetVariationParam(quic_trial_params, "host_whitelist");
  std::vector<std::string> host_vector = base::SplitString(
      host_allowlist, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  return base::flat_set<std::string>(std::move(host_vector));
}

int GetInitialDelayForBrokenAlternativeServiceSeconds(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(
          GetVariationParam(
              quic_trial_params,
              "initial_delay_for_broken_alternative_service_seconds"),
          &value)) {
    return value;
  }
  return 0;
}

bool DelayMainJobWithAvailableSpdySession(
    const VariationParameters& quic_trial_params) {
  return base::EqualsCaseInsensitiveASCII(
      GetVariationParam(quic_trial_params,
                        "delay_main_job_with_available_spdy_session"),
      "true");
}

bool IsOriginFrameEnabled(const VariationParameters& quic_trial_params) {
  return !base::EqualsCaseInsensitiveASCII(
      GetVariationParam(quic_trial_params, "enable_origin_frame"), "false");
}

bool IsDnsSkippedWithOriginFrame(const VariationParameters& quic_trial_params) {
  return !base::EqualsCaseInsensitiveASCII(
      GetVariationParam(quic_trial_params, "skip_dns_with_origin_frame"),
      "false");
}

bool IgnoreIpMatchingWhenFindingExistingSessions(
    const VariationParameters& quic_trial_params) {
  return base::EqualsCaseInsensitiveASCII(
      GetVariationParam(quic_trial_params,
                        "ignore_ip_matching_when_finding_existing_sessions"),
      "true");
}

bool AllowServerMigration(const VariationParameters& quic_trial_params) {
  return !base::EqualsCaseInsensitiveASCII(
      GetVariationParam(quic_trial_params, "allow_server_migration"), "false");
}

void SetQuicFlags(const VariationParameters& quic_trial_params) {
  std::string flags_list =
      GetVariationParam(quic_trial_params, "set_quic_flags");
  for (const auto& flag : base::SplitString(
           flags_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    std::vector<std::string> tokens = base::SplitString(
        flag, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (tokens.size() != 2)
      continue;
    net::SetQuicFlagByName(tokens[0], tokens[1]);
  }
}

size_t GetQuicMaxPacketLength(const VariationParameters& quic_trial_params) {
  unsigned value;
  if (base::StringToUint(
          GetVariationParam(quic_trial_params, "max_packet_length"), &value)) {
    return value;
  }
  return 0;
}

quic::ParsedQuicVersionVector GetQuicVersions(
    const VariationParameters& quic_trial_params) {
  std::string trial_versions_str =
      GetVariationParam(quic_trial_params, "quic_version");
  quic::ParsedQuicVersionVector trial_versions =
      quic::ParseQuicVersionVectorString(trial_versions_str);
  quic::ParsedQuicVersionVector filtered_versions;
  quic::ParsedQuicVersionVector obsolete_versions = net::ObsoleteQuicVersions();
  bool found_obsolete_version = false;
  for (const quic::ParsedQuicVersion& version : trial_versions) {
    if (!base::Contains(obsolete_versions, version)) {
      filtered_versions.push_back(version);
    } else {
      found_obsolete_version = true;
    }
  }
  if (found_obsolete_version) {
    UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.FinchObsoleteVersion", true);
  }
  return filtered_versions;
}

bool AreQuicParamsValid(const base::CommandLine& command_line,
                        std::string_view quic_trial_group,
                        const VariationParameters& quic_trial_params) {
  if (command_line.HasSwitch(variations::switches::kForceFieldTrialParams)) {
    // Skip validation of params from the command line.
    return true;
  }
  if (!base::EqualsCaseInsensitiveASCII(
          GetVariationParam(quic_trial_params, "enable_quic"), "true")) {
    // Params that don't explicitly enable QUIC do not carry channel or epoch.
    return true;
  }
  const std::string channel_string =
      GetVariationParam(quic_trial_params, "channel");
  if (channel_string.length() != 1) {
    // Params without a valid channel are invalid.
    return false;
  }
  const std::string epoch_string =
      GetVariationParam(quic_trial_params, "epoch");
  if (epoch_string.length() != 8) {
    // Params without a valid epoch are invalid.
    return false;
  }
  int epoch;
  if (!base::StringToInt(epoch_string, &epoch)) {
    // Failed to parse epoch as int.
    return false;
  }
  if (epoch < 20211025) {
    // All channels currently have an epoch of at least this value. This
    // can be confirmed by checking the internal QUIC field trial config file.
    return false;
  }
  return true;
}

void ConfigureQuicParams(const base::CommandLine& command_line,
                         std::string_view quic_trial_group,
                         const VariationParameters& quic_trial_params,
                         bool is_quic_force_disabled,
                         net::HttpNetworkSessionParams* params,
                         net::QuicParams* quic_params) {
  if (ShouldDisableQuic(quic_trial_group, quic_trial_params,
                        is_quic_force_disabled)) {
    params->enable_quic = false;
  }

  const bool params_are_valid =
      AreQuicParamsValid(command_line, quic_trial_group, quic_trial_params);
  UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.FinchConfigIsValid", params_are_valid);
  if (!params_are_valid) {
    // Skip parsing of invalid params.
    return;
  }

  quic_params->retry_without_alt_svc_on_quic_errors =
      ShouldRetryWithoutAltSvcOnQuicErrors(quic_trial_params);

  if (params->enable_quic) {
    quic_params->connection_options =
        GetQuicConnectionOptions(quic_trial_params);
    quic_params->client_connection_options =
        GetQuicClientConnectionOptions(quic_trial_params);
    quic_params->close_sessions_on_ip_change =
        ShouldQuicCloseSessionsOnIpChange(quic_trial_params);
    quic_params->goaway_sessions_on_ip_change =
        ShouldQuicGoAwaySessionsOnIpChange(quic_trial_params);
    int idle_connection_timeout_seconds =
        GetQuicIdleConnectionTimeoutSeconds(quic_trial_params);
    if (idle_connection_timeout_seconds != 0) {
      quic_params->idle_connection_timeout =
          base::Seconds(idle_connection_timeout_seconds);
    }
    int reduced_ping_timeout_seconds =
        GetQuicReducedPingTimeoutSeconds(quic_trial_params);
    if (reduced_ping_timeout_seconds > 0 &&
        reduced_ping_timeout_seconds < quic::kPingTimeoutSecs) {
      quic_params->reduced_ping_timeout =
          base::Seconds(reduced_ping_timeout_seconds);
    }
    int max_time_before_crypto_handshake_seconds =
        GetQuicMaxTimeBeforeCryptoHandshakeSeconds(quic_trial_params);
    if (max_time_before_crypto_handshake_seconds > 0) {
      quic_params->max_time_before_crypto_handshake =
          base::Seconds(max_time_before_crypto_handshake_seconds);
    }
    int max_idle_time_before_crypto_handshake_seconds =
        GetQuicMaxIdleTimeBeforeCryptoHandshakeSeconds(quic_trial_params);
    if (max_idle_time_before_crypto_handshake_seconds > 0) {
      quic_params->max_idle_time_before_crypto_handshake =
          base::Seconds(max_idle_time_before_crypto_handshake_seconds);
    }
    quic_params->estimate_initial_rtt =
        ShouldQuicEstimateInitialRtt(quic_trial_params);
    quic_params->migrate_sessions_on_network_change_v2 =
        ShouldQuicMigrateSessionsOnNetworkChangeV2(quic_trial_params);
    quic_params->use_new_alps_codepoint =
        ShouldQuicUseNewAlpsCodepoint(quic_trial_params);
    quic_params->migrate_sessions_early_v2 =
        ShouldQuicMigrateSessionsEarlyV2(quic_trial_params);
    quic_params->allow_port_migration =
        ShouldQuicAllowPortMigration(quic_trial_params);
    quic_params->retry_on_alternate_network_before_handshake =
        ShouldQuicRetryOnAlternateNetworkBeforeHandshake(quic_trial_params);
    int initial_rtt_for_handshake_milliseconds =
        GetQuicInitialRttForHandshakeMilliseconds(quic_trial_params);
    if (initial_rtt_for_handshake_milliseconds > 0) {
      quic_params->initial_rtt_for_handshake =
          base::Milliseconds(initial_rtt_for_handshake_milliseconds);
    }

    quic_params->disable_tls_zero_rtt =
        ShouldQuicDisableTlsZeroRtt(quic_trial_params);

    quic_params->disable_gquic_zero_rtt =
        ShouldQuicDisableGQuicZeroRtt(quic_trial_params);

    int retransmittable_on_wire_timeout_milliseconds =
        GetQuicRetransmittableOnWireTimeoutMilliseconds(quic_trial_params);
    if (retransmittable_on_wire_timeout_milliseconds > 0) {
      quic_params->retransmittable_on_wire_timeout =
          base::Milliseconds(retransmittable_on_wire_timeout_milliseconds);
    }
    quic_params->migrate_idle_sessions =
        ShouldQuicMigrateIdleSessions(quic_trial_params);
    int idle_session_migration_period_seconds =
        GetQuicIdleSessionMigrationPeriodSeconds(quic_trial_params);
    if (idle_session_migration_period_seconds > 0) {
      quic_params->idle_session_migration_period =
          base::Seconds(idle_session_migration_period_seconds);
    }
    int multi_port_probing_interval =
        GetMultiPortProbingInterval(quic_trial_params);
    if (multi_port_probing_interval > 0) {
      quic_params->multi_port_probing_interval = multi_port_probing_interval;
    }
    int max_time_on_non_default_network_seconds =
        GetQuicMaxTimeOnNonDefaultNetworkSeconds(quic_trial_params);
    if (max_time_on_non_default_network_seconds > 0) {
      quic_params->max_time_on_non_default_network =
          base::Seconds(max_time_on_non_default_network_seconds);
    }
    int max_migrations_to_non_default_network_on_write_error =
        GetQuicMaxNumMigrationsToNonDefaultNetworkOnWriteError(
            quic_trial_params);
    if (max_migrations_to_non_default_network_on_write_error > 0) {
      quic_params->max_migrations_to_non_default_network_on_write_error =
          max_migrations_to_non_default_network_on_write_error;
    }
    int max_migrations_to_non_default_network_on_path_degrading =
        GetQuicMaxNumMigrationsToNonDefaultNetworkOnPathDegrading(
            quic_trial_params);
    if (max_migrations_to_non_default_network_on_path_degrading > 0) {
      quic_params->max_migrations_to_non_default_network_on_path_degrading =
          max_migrations_to_non_default_network_on_path_degrading;
    }
    params->quic_host_allowlist = GetQuicHostAllowlist(quic_trial_params);
    const int initial_delay_for_broken_alternative_service_seconds =
        GetInitialDelayForBrokenAlternativeServiceSeconds(quic_trial_params);
    if (initial_delay_for_broken_alternative_service_seconds > 0) {
      quic_params->initial_delay_for_broken_alternative_service =
          base::Seconds(initial_delay_for_broken_alternative_service_seconds);
    }
    quic_params->exponential_backoff_on_initial_delay =
        GetExponentialBackOffOnInitialDelay(quic_trial_params);
    if (DelayMainJobWithAvailableSpdySession(quic_trial_params)) {
      quic_params->delay_main_job_with_available_spdy_session = true;
    }
    quic_params->report_ecn = ShouldQuicReportEcn(quic_trial_params);
    quic_params->enable_origin_frame = IsOriginFrameEnabled(quic_trial_params);
    quic_params->skip_dns_with_origin_frame =
        IsDnsSkippedWithOriginFrame(quic_trial_params);
    quic_params->ignore_ip_matching_when_finding_existing_sessions =
        IgnoreIpMatchingWhenFindingExistingSessions(quic_trial_params);
    quic_params->allow_server_migration =
        AllowServerMigration(quic_trial_params);
    SetQuicFlags(quic_trial_params);
  }

  size_t max_packet_length = GetQuicMaxPacketLength(quic_trial_params);
  if (max_packet_length != 0) {
    quic_params->max_packet_length = max_packet_length;
  }

  quic::ParsedQuicVersionVector supported_versions =
      GetQuicVersions(quic_trial_params);
  if (!supported_versions.empty())
    quic_params->supported_versions = supported_versions;
}

}  // anonymous namespace

namespace network_session_configurator {

void ParseCommandLineAndFieldTrials(const base::CommandLine& command_line,
                                    bool is_quic_force_disabled,
                                    net::HttpNetworkSessionParams* params,
                                    net::QuicParams* quic_params) {
  is_quic_force_disabled |= command_line.HasSwitch(switches::kDisableQuic);

  std::string quic_trial_group =
      base::FieldTrialList::FindFullName(kQuicFieldTrialName);
  VariationParameters quic_trial_params;
  if (!base::GetFieldTrialParams(kQuicFieldTrialName, &quic_trial_params)) {
    quic_trial_params.clear();
  }
  ConfigureQuicParams(command_line, quic_trial_group, quic_trial_params,
                      is_quic_force_disabled, params, quic_params);

  std::string http2_trial_group =
      base::FieldTrialList::FindFullName(kHttp2FieldTrialName);
  VariationParameters http2_trial_params;
  if (!base::GetFieldTrialParams(kHttp2FieldTrialName, &http2_trial_params)) {
    http2_trial_params.clear();
  }
  ConfigureHttp2Params(command_line, http2_trial_group, http2_trial_params,
                       params);

  // Command line flags override field trials.
  if (command_line.HasSwitch(switches::kDisableHttp2))
    params->enable_http2 = false;

  if (params->enable_quic) {
    if (command_line.HasSwitch(switches::kQuicConnectionOptions)) {
      quic_params->connection_options = quic::ParseQuicTagVector(
          command_line.GetSwitchValueASCII(switches::kQuicConnectionOptions));
    }
    if (command_line.HasSwitch(switches::kQuicClientConnectionOptions)) {
      quic_params->client_connection_options =
          quic::ParseQuicTagVector(command_line.GetSwitchValueASCII(
              switches::kQuicClientConnectionOptions));
    }

    if (command_line.HasSwitch(switches::kQuicMaxPacketLength)) {
      unsigned value;
      if (base::StringToUint(
              command_line.GetSwitchValueASCII(switches::kQuicMaxPacketLength),
              &value)) {
        quic_params->max_packet_length = value;
      }
    }

    if (command_line.HasSwitch(switches::kQuicVersion)) {
      quic::ParsedQuicVersionVector supported_versions =
          quic::ParseQuicVersionVectorString(
              command_line.GetSwitchValueASCII(switches::kQuicVersion));
      if (!supported_versions.empty())
        quic_params->supported_versions = supported_versions;
    }

    if (command_line.HasSwitch(switches::kOriginToForceQuicOn)) {
      std::string origins =
          command_line.GetSwitchValueASCII(switches::kOriginToForceQuicOn);
      for (const std::string& host_port : base::SplitString(
               origins, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
        if (host_port == "*")
          quic_params->origins_to_force_quic_on.insert(net::HostPortPair());
        net::HostPortPair quic_origin =
            net::HostPortPair::FromString(host_port);
        if (!quic_origin.IsEmpty())
          quic_params->origins_to_force_quic_on.insert(quic_origin);
      }
    }

    if (command_line.HasSwitch(switches::kWebTransportDeveloperMode)) {
      quic_params->webtransport_developer_mode = true;
    }
  }

  // Parameters only controlled by command line.
  if (command_line.HasSwitch(switches::kEnableUserAlternateProtocolPorts)) {
    params->enable_user_alternate_protocol_ports = true;
  }
  if (command_line.HasSwitch(switches::kIgnoreCertificateErrors)) {
    params->ignore_certificate_errors = true;
  }
  if (command_line.HasSwitch(switches::kTestingFixedHttpPort)) {
    params->testing_fixed_http_port =
        GetSwitchValueAsInt(command_line, switches::kTestingFixedHttpPort);
  }
  if (command_line.HasSwitch(switches::kTestingFixedHttpsPort)) {
    params->testing_fixed_https_port =
        GetSwitchValueAsInt(command_line, switches::kTestingFixedHttpsPort);
  }

  if (command_line.HasSwitch(switches::kHostRules)) {
    params->host_mapping_rules.SetRulesFromString(
        command_line.GetSwitchValueASCII(switches::kHostRules));
  }
}

net::URLRequestContextBuilder::HttpCacheParams::Type ChooseCacheType() {
  if constexpr (disk_cache::IsSimpleBackendEnabledByDefaultPlatform()) {
    return net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE;
  }
  if (disk_cache::InSimpleBackendExperimentGroup()) {
    return net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE;
  }
  return net::URLRequestContextBuilder::HttpCacheParams::DISK_BLOCKFILE;
}

}  // namespace network_session_configurator
