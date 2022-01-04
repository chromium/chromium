// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/url_request_context_config.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/cronet/stale_host_resolver.h"
#include "net/base/address_family.h"
#include "net/cert/caching_cert_verifier.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/log/net_log.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/reporting/reporting_policy.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_key_logger_impl.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/core/quic_tag.h"
#include "net/url_request/url_request_context_builder.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/reporting/reporting_policy.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

namespace cronet {

namespace {

// Name of disk cache directory.
const base::FilePath::CharType kDiskCacheDirectoryName[] =
    FILE_PATH_LITERAL("disk_cache");
// TODO(xunjieli): Refactor constants in io_thread.cc.
const char kQuicFieldTrialName[] = "QUIC";
const char kQuicConnectionOptions[] = "connection_options";
const char kQuicClientConnectionOptions[] = "client_connection_options";
const char kQuicStoreServerConfigsInProperties[] =
    "store_server_configs_in_properties";
const char kQuicMaxServerConfigsStoredInProperties[] =
    "max_server_configs_stored_in_properties";
const char kQuicIdleConnectionTimeoutSeconds[] =
    "idle_connection_timeout_seconds";
const char kQuicMaxTimeBeforeCryptoHandshakeSeconds[] =
    "max_time_before_crypto_handshake_seconds";
const char kQuicMaxIdleTimeBeforeCryptoHandshakeSeconds[] =
    "max_idle_time_before_crypto_handshake_seconds";
const char kQuicCloseSessionsOnIpChange[] = "close_sessions_on_ip_change";
const char kQuicGoAwaySessionsOnIpChange[] = "goaway_sessions_on_ip_change";
const char kQuicAllowServerMigration[] = "allow_server_migration";
const char kQuicMigrateSessionsOnNetworkChangeV2[] =
    "migrate_sessions_on_network_change_v2";
const char kQuicMigrateIdleSessions[] = "migrate_idle_sessions";
const char kQuicRetransmittableOnWireTimeoutMilliseconds[] =
    "retransmittable_on_wire_timeout_milliseconds";
const char kQuicIdleSessionMigrationPeriodSeconds[] =
    "idle_session_migration_period_seconds";
const char kQuicMaxTimeOnNonDefaultNetworkSeconds[] =
    "max_time_on_non_default_network_seconds";
const char kQuicMaxMigrationsToNonDefaultNetworkOnWriteError[] =
    "max_migrations_to_non_default_network_on_write_error";
const char kQuicMaxMigrationsToNonDefaultNetworkOnPathDegrading[] =
    "max_migrations_to_non_default_network_on_path_degrading";
const char kQuicUserAgentId[] = "user_agent_id";
const char kQuicMigrateSessionsEarlyV2[] = "migrate_sessions_early_v2";
const char kQuicRetryOnAlternateNetworkBeforeHandshake[] =
    "retry_on_alternate_network_before_handshake";
const char kQuicRaceStaleDNSOnConnection[] = "race_stale_dns_on_connection";
const char kQuicDisableBidirectionalStreams[] =
    "quic_disable_bidirectional_streams";
const char kQuicHostWhitelist[] = "host_whitelist";
const char kQuicEnableSocketRecvOptimization[] =
    "enable_socket_recv_optimization";
const char kQuicVersion[] = "quic_version";
const char kQuicObsoleteVersionsAllowed[] = "obsolete_versions_allowed";
const char kQuicFlags[] = "set_quic_flags";
const char kQuicIOSNetworkServiceType[] = "ios_network_service_type";

// AsyncDNS experiment dictionary name.
const char kAsyncDnsFieldTrialName[] = "AsyncDNS";
// Name of boolean to enable AsyncDNS experiment.
const char kAsyncDnsEnable[] = "enable";

// Stale DNS (StaleHostResolver) experiment dictionary name.
const char kStaleDnsFieldTrialName[] = "StaleDNS";
// Name of boolean to enable stale DNS experiment.
const char kStaleDnsEnable[] = "enable";
// Name of integer delay in milliseconds before a stale DNS result will be
// used.
const char kStaleDnsDelayMs[] = "delay_ms";
// Name of integer maximum age (past expiration) in milliseconds of a stale DNS
// result that will be used, or 0 for no limit.
const char kStaleDnsMaxExpiredTimeMs[] = "max_expired_time_ms";
// Name of integer maximum times each stale DNS result can be used, or 0 for no
// limit.
const char kStaleDnsMaxStaleUses[] = "max_stale_uses";
// Name of boolean to allow stale DNS results from other networks to be used on
// the current network.
const char kStaleDnsAllowOtherNetwork[] = "allow_other_network";
// Name of boolean to enable persisting the DNS cache to disk.
const char kStaleDnsPersist[] = "persist_to_disk";
// Name of integer minimum time in milliseconds between writes to disk for DNS
// cache persistence. Default value is one minute. Only relevant if
// "persist_to_disk" is true.
const char kStaleDnsPersistTimer[] = "persist_delay_ms";
// Name of boolean to allow use of stale DNS results when network resolver
// returns ERR_NAME_NOT_RESOLVED.
const char kStaleDnsUseStaleOnNameNotResolved[] =
    "use_stale_on_name_not_resolved";

// Rules to override DNS resolution. Intended for testing.
// See explanation of format in net/dns/mapped_host_resolver.h.
const char kHostResolverRulesFieldTrialName[] = "HostResolverRules";
const char kHostResolverRules[] = "host_resolver_rules";

// NetworkQualityEstimator (NQE) experiment dictionary name.
const char kNetworkQualityEstimatorFieldTrialName[] = "NetworkQualityEstimator";

// Network Error Logging experiment dictionary name.
const char kNetworkErrorLoggingFieldTrialName[] = "NetworkErrorLogging";
// Name of boolean to enable Reporting API.
const char kNetworkErrorLoggingEnable[] = "enable";
// Name of list of preloaded "Report-To" headers.
const char kNetworkErrorLoggingPreloadedReportToHeaders[] =
    "preloaded_report_to_headers";
// Name of list of preloaded "NEL" headers.
const char kNetworkErrorLoggingPreloadedNELHeaders[] = "preloaded_nel_headers";
// Name of key (for above two lists) for header origin.
const char kNetworkErrorLoggingOrigin[] = "origin";
// Name of key (for above two lists) for header value.
const char kNetworkErrorLoggingValue[] = "value";

// Disable IPv6 when on WiFi. This is a workaround for a known issue on certain
// Android phones, and should not be necessary when not on one of those devices.
// See https://crbug.com/696569 for details.
const char kDisableIPv6OnWifi[] = "disable_ipv6_on_wifi";

const char kSSLKeyLogFile[] = "ssl_key_log_file";

const char kGoAwayOnPathDegrading[] = "go_away_on_path_degrading";

const char kAllowPortMigration[] = "allow_port_migration";

const char kDisableTlsZeroRtt[] = "disable_tls_zero_rtt";

// Whether SPDY sessions should be closed or marked as going away upon relevant
// network changes. When not specified, /net behavior varies depending on the
// underlying OS.
const char kSpdyGoAwayOnIpChange[] = "spdy_go_away_on_ip_change";

// "goaway_sessions_on_ip_change" is default on for iOS unless overridden via
// experimental options explicitly.
#if defined(OS_IOS)
const bool kDefaultQuicGoAwaySessionsOnIpChange = true;
#else
const bool kDefaultQuicGoAwaySessionsOnIpChange = false;
#endif

// Serializes a base::Value into a string that can be used as the value of
// JFV-encoded HTTP header [1].  If |value| is a list, we remove the outermost
// [] delimiters from the result.
//
// [1] https://tools.ietf.org/html/draft-reschke-http-jfv
std::string SerializeJFVHeader(const base::Value& value) {
  std::string result;
  if (!base::JSONWriter::Write(value, &result))
    return std::string();
  if (value.is_list()) {
    DCHECK(result.size() >= 2);
    return result.substr(1, result.size() - 2);
  }
  return result;
}

std::vector<URLRequestContextConfig::PreloadedNelAndReportingHeader>
ParseNetworkErrorLoggingHeaders(
    base::Value::ConstListView preloaded_headers_config) {
  std::vector<URLRequestContextConfig::PreloadedNelAndReportingHeader> result;
  for (const auto& preloaded_header_config : preloaded_headers_config) {
    if (!preloaded_header_config.is_dict())
      continue;

    auto* origin_config = preloaded_header_config.FindKeyOfType(
        kNetworkErrorLoggingOrigin, base::Value::Type::STRING);
    if (!origin_config)
      continue;
    GURL origin_url(origin_config->GetString());
    if (!origin_url.is_valid())
      continue;
    auto origin = url::Origin::Create(origin_url);

    auto* value = preloaded_header_config.FindKey(kNetworkErrorLoggingValue);
    if (!value)
      continue;

    result.push_back(URLRequestContextConfig::PreloadedNelAndReportingHeader(
        origin, SerializeJFVHeader(*value)));
  }
  return result;
}

}  // namespace

URLRequestContextConfig::QuicHint::QuicHint(const std::string& host,
                                            int port,
                                            int alternate_port)
    : host(host), port(port), alternate_port(alternate_port) {}

URLRequestContextConfig::QuicHint::~QuicHint() {}

URLRequestContextConfig::Pkp::Pkp(const std::string& host,
                                  bool include_subdomains,
                                  const base::Time& expiration_date)
    : host(host),
      include_subdomains(include_subdomains),
      expiration_date(expiration_date) {}

URLRequestContextConfig::Pkp::~Pkp() {}

URLRequestContextConfig::PreloadedNelAndReportingHeader::
    PreloadedNelAndReportingHeader(const url::Origin& origin, std::string value)
    : origin(origin), value(std::move(value)) {}

URLRequestContextConfig::PreloadedNelAndReportingHeader::
    ~PreloadedNelAndReportingHeader() = default;

URLRequestContextConfig::URLRequestContextConfig(
    bool enable_quic,
    const std::string& quic_user_agent_id,
    bool enable_spdy,
    bool enable_brotli,
    HttpCacheType http_cache,
    int http_cache_max_size,
    bool load_disable_cache,
    const std::string& storage_path,
    const std::string& accept_language,
    const std::string& user_agent,
    const std::string& experimental_options,
    std::unique_ptr<net::CertVerifier> mock_cert_verifier,
    bool enable_network_quality_estimator,
    bool bypass_public_key_pinning_for_local_trust_anchors,
    absl::optional<double> network_thread_priority)
    : enable_quic(enable_quic),
      quic_user_agent_id(quic_user_agent_id),
      enable_spdy(enable_spdy),
      enable_brotli(enable_brotli),
      http_cache(http_cache),
      http_cache_max_size(http_cache_max_size),
      load_disable_cache(load_disable_cache),
      storage_path(storage_path),
      accept_language(accept_language),
      user_agent(user_agent),
      mock_cert_verifier(std::move(mock_cert_verifier)),
      enable_network_quality_estimator(enable_network_quality_estimator),
      bypass_public_key_pinning_for_local_trust_anchors(
          bypass_public_key_pinning_for_local_trust_anchors),
      network_thread_priority(network_thread_priority),
      experimental_options(experimental_options) {}

URLRequestContextConfig::~URLRequestContextConfig() {}

bool URLRequestContextConfig::ParseAndSetExperimentalOptions(
    net::URLRequestContextBuilder* context_builder,
    net::HttpNetworkSessionParams* session_params,
    net::QuicParams* quic_params) {
  if (experimental_options.empty())
    return true;

  DVLOG(1) << "Experimental Options:" << experimental_options;
  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(experimental_options);

  if (!parsed_json.value) {
    LOG(ERROR) << "Parsing experimental options failed: '"
               << experimental_options << "', error "
               << parsed_json.error_message;
    return false;
  }

  std::unique_ptr<base::Value> root =
      base::Value::ToUniquePtrValue(std::move(*parsed_json.value));

  std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(std::move(root));

  if (!dict) {
    LOG(ERROR) << "Experimental options string is not a dictionary: "
               << experimental_options;
    return false;
  }

  bool async_dns_enable = false;
  bool stale_dns_enable = false;
  bool host_resolver_rules_enable = false;
  bool disable_ipv6_on_wifi = false;
  bool nel_enable = false;

  effective_experimental_options = dict->CreateDeepCopy();
  StaleHostResolver::StaleOptions stale_dns_options;
  std::string host_resolver_rules_string;

  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    if (it.key() == kQuicFieldTrialName) {
      const base::DictionaryValue* quic_args = nullptr;
      if (!it.value().GetAsDictionary(&quic_args)) {
        LOG(ERROR) << "Quic config params \"" << it.value()
                   << "\" is not a dictionary value";
        effective_experimental_options->RemoveKey(it.key());
        continue;
      }

      std::string quic_version_string;
      if (quic_args->GetString(kQuicVersion, &quic_version_string)) {
        quic::ParsedQuicVersionVector supported_versions =
            quic::ParseQuicVersionVectorString(quic_version_string);
        absl::optional<bool> obsolete_versions_allowed =
            quic_args->FindBoolKey(kQuicObsoleteVersionsAllowed);
        if (!obsolete_versions_allowed || !*obsolete_versions_allowed) {
          quic::ParsedQuicVersionVector filtered_versions;
          quic::ParsedQuicVersionVector obsolete_versions =
              net::ObsoleteQuicVersions();
          for (const quic::ParsedQuicVersion& version : supported_versions) {
            if (version == quic::ParsedQuicVersion::Q043()) {
              // TODO(dschinazi) Remove this special-casing of Q043 once we no
              // longer have cronet applications that require it.
              filtered_versions.push_back(version);
              continue;
            }
            if (std::find(obsolete_versions.begin(), obsolete_versions.end(),
                          version) == obsolete_versions.end()) {
              filtered_versions.push_back(version);
            }
          }
          supported_versions = filtered_versions;
        }
        if (!supported_versions.empty())
          quic_params->supported_versions = supported_versions;
      }

      std::string quic_connection_options;
      if (quic_args->GetString(kQuicConnectionOptions,
                               &quic_connection_options)) {
        quic_params->connection_options =
            quic::ParseQuicTagVector(quic_connection_options);
      }

      std::string quic_client_connection_options;
      if (quic_args->GetString(kQuicClientConnectionOptions,
                               &quic_client_connection_options)) {
        quic_params->client_connection_options =
            quic::ParseQuicTagVector(quic_client_connection_options);
      }

      // TODO(rtenneti): Delete this option after apps stop using it.
      // Added this for backward compatibility.
      if (quic_args->FindBoolKey(kQuicStoreServerConfigsInProperties)) {
        quic_params->max_server_configs_stored_in_properties =
            net::kDefaultMaxQuicServerEntries;
      }

      absl::optional<int> quic_max_server_configs_stored_in_properties =
          quic_args->FindIntKey(kQuicMaxServerConfigsStoredInProperties);
      if (quic_max_server_configs_stored_in_properties) {
        quic_params->max_server_configs_stored_in_properties =
            static_cast<size_t>(*quic_max_server_configs_stored_in_properties);
      }

      absl::optional<int> quic_idle_connection_timeout_seconds =
          quic_args->FindIntKey(kQuicIdleConnectionTimeoutSeconds);
      if (quic_idle_connection_timeout_seconds) {
        quic_params->idle_connection_timeout =
            base::Seconds(*quic_idle_connection_timeout_seconds);
      }

      absl::optional<int> quic_max_time_before_crypto_handshake_seconds =
          quic_args->FindIntKey(kQuicMaxTimeBeforeCryptoHandshakeSeconds);
      if (quic_max_time_before_crypto_handshake_seconds) {
        quic_params->max_time_before_crypto_handshake =
            base::Seconds(*quic_max_time_before_crypto_handshake_seconds);
      }

      absl::optional<int> quic_max_idle_time_before_crypto_handshake_seconds =
          quic_args->FindIntKey(kQuicMaxIdleTimeBeforeCryptoHandshakeSeconds);
      if (quic_max_idle_time_before_crypto_handshake_seconds) {
        quic_params->max_idle_time_before_crypto_handshake =
            base::Seconds(*quic_max_idle_time_before_crypto_handshake_seconds);
      }

      absl::optional<bool> quic_close_sessions_on_ip_change =
          quic_args->FindBoolKey(kQuicCloseSessionsOnIpChange);
      if (quic_close_sessions_on_ip_change) {
        quic_params->close_sessions_on_ip_change =
            *quic_close_sessions_on_ip_change;
        if (*quic_close_sessions_on_ip_change &&
            kDefaultQuicGoAwaySessionsOnIpChange) {
          // "close_sessions_on_ip_change" and "goaway_sessions_on_ip_change"
          // are mutually exclusive. Turn off the goaway option which is
          // default on for iOS if "close_sessions_on_ip_change" is set via
          // experimental options.
          quic_params->goaway_sessions_on_ip_change = false;
        }
      }

      quic_params->goaway_sessions_on_ip_change =
          quic_args->FindBoolKey(kQuicGoAwaySessionsOnIpChange)
              .value_or(quic_params->goaway_sessions_on_ip_change);
      quic_params->go_away_on_path_degrading =
          quic_args->FindBoolKey(kGoAwayOnPathDegrading)
              .value_or(quic_params->go_away_on_path_degrading);
      quic_params->allow_server_migration =
          quic_args->FindBoolKey(kQuicAllowServerMigration)
              .value_or(quic_params->allow_server_migration);

      std::string user_agent_id;
      if (quic_args->GetString(kQuicUserAgentId, &user_agent_id)) {
        quic_params->user_agent_id = user_agent_id;
      }

      quic_params->enable_socket_recv_optimization =
          quic_args->FindBoolKey(kQuicEnableSocketRecvOptimization)
              .value_or(quic_params->enable_socket_recv_optimization);

      absl::optional<bool> quic_migrate_sessions_on_network_change_v2_in =
          quic_args->FindBoolKey(kQuicMigrateSessionsOnNetworkChangeV2);
      if (quic_migrate_sessions_on_network_change_v2_in.has_value()) {
        quic_params->migrate_sessions_on_network_change_v2 =
            quic_migrate_sessions_on_network_change_v2_in.value();

        absl::optional<int> quic_max_time_on_non_default_network_seconds =
            quic_args->FindIntKey(kQuicMaxTimeOnNonDefaultNetworkSeconds);
        if (quic_max_time_on_non_default_network_seconds) {
          quic_params->max_time_on_non_default_network =
              base::Seconds(*quic_max_time_on_non_default_network_seconds);
        }

        absl::optional<int>
            quic_max_migrations_to_non_default_network_on_write_error =
                quic_args->FindIntKey(
                    kQuicMaxMigrationsToNonDefaultNetworkOnWriteError);
        if (quic_max_migrations_to_non_default_network_on_write_error) {
          quic_params->max_migrations_to_non_default_network_on_write_error =
              *quic_max_migrations_to_non_default_network_on_write_error;
        }

        absl::optional<int>
            quic_max_migrations_to_non_default_network_on_path_degrading =
                quic_args->FindIntKey(
                    kQuicMaxMigrationsToNonDefaultNetworkOnPathDegrading);
        if (quic_max_migrations_to_non_default_network_on_path_degrading) {
          quic_params->max_migrations_to_non_default_network_on_path_degrading =
              *quic_max_migrations_to_non_default_network_on_path_degrading;
        }
      }

      absl::optional<bool> quic_migrate_idle_sessions_in =
          quic_args->FindBoolKey(kQuicMigrateIdleSessions);
      if (quic_migrate_idle_sessions_in.has_value()) {
        quic_params->migrate_idle_sessions =
            quic_migrate_idle_sessions_in.value();

        absl::optional<int> quic_idle_session_migration_period_seconds =
            quic_args->FindIntKey(kQuicIdleSessionMigrationPeriodSeconds);
        if (quic_idle_session_migration_period_seconds) {
          quic_params->idle_session_migration_period =
              base::Seconds(*quic_idle_session_migration_period_seconds);
        }
      }

      absl::optional<bool> quic_migrate_sessions_early_v2_in =
          quic_args->FindBoolKey(kQuicMigrateSessionsEarlyV2);
      if (quic_migrate_sessions_early_v2_in.has_value()) {
        quic_params->migrate_sessions_early_v2 =
            quic_migrate_sessions_early_v2_in.value();
      }

      absl::optional<int> quic_retransmittable_on_wire_timeout_milliseconds =
          quic_args->FindIntKey(kQuicRetransmittableOnWireTimeoutMilliseconds);
      if (quic_retransmittable_on_wire_timeout_milliseconds) {
        quic_params->retransmittable_on_wire_timeout = base::Milliseconds(
            *quic_retransmittable_on_wire_timeout_milliseconds);
      }

      quic_params->retry_on_alternate_network_before_handshake =
          quic_args->FindBoolKey(kQuicRetryOnAlternateNetworkBeforeHandshake)
              .value_or(
                  quic_params->retry_on_alternate_network_before_handshake);

      quic_params->race_stale_dns_on_connection =
          quic_args->FindBoolKey(kQuicRaceStaleDNSOnConnection)
              .value_or(quic_params->race_stale_dns_on_connection);

      quic_params->allow_port_migration =
          quic_args->FindBoolKey(kAllowPortMigration)
              .value_or(quic_params->allow_port_migration);

      quic_params->disable_tls_zero_rtt =
          quic_args->FindBoolKey(kDisableTlsZeroRtt)
              .value_or(quic_params->disable_tls_zero_rtt);

      quic_params->disable_bidirectional_streams =
          quic_args->FindBoolKey(kQuicDisableBidirectionalStreams)
              .value_or(quic_params->disable_bidirectional_streams);

      std::string quic_host_allowlist;
      if (quic_args->GetString(kQuicHostWhitelist, &quic_host_allowlist)) {
        std::vector<std::string> host_vector =
            base::SplitString(quic_host_allowlist, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_ALL);
        session_params->quic_host_allowlist.clear();
        for (const std::string& host : host_vector) {
          session_params->quic_host_allowlist.insert(host);
        }
      }

      std::string quic_flags;
      if (quic_args->GetString(kQuicFlags, &quic_flags)) {
        for (const auto& flag :
             base::SplitString(quic_flags, ",", base::TRIM_WHITESPACE,
                               base::SPLIT_WANT_ALL)) {
          std::vector<std::string> tokens = base::SplitString(
              flag, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
          if (tokens.size() != 2)
            continue;
          SetQuicFlagByName(tokens[0], tokens[1]);
        }
      }

      absl::optional<int> quic_ios_network_service_type =
          quic_args->FindIntKey(kQuicIOSNetworkServiceType);
      if (quic_ios_network_service_type) {
        quic_params->ios_network_service_type = *quic_ios_network_service_type;
      }
    } else if (it.key() == kAsyncDnsFieldTrialName) {
      const base::DictionaryValue* async_dns_args = nullptr;
      if (!it.value().GetAsDictionary(&async_dns_args)) {
        LOG(ERROR) << "\"" << it.key() << "\" config params \"" << it.value()
                   << "\" is not a dictionary value";
        effective_experimental_options->RemoveKey(it.key());
        continue;
      }
      async_dns_enable = async_dns_args->FindBoolKey(kAsyncDnsEnable)
                             .value_or(async_dns_enable);
    } else if (it.key() == kStaleDnsFieldTrialName) {
      const base::DictionaryValue* stale_dns_args = nullptr;
      if (!it.value().GetAsDictionary(&stale_dns_args)) {
        LOG(ERROR) << "\"" << it.key() << "\" config params \"" << it.value()
                   << "\" is not a dictionary value";
        effective_experimental_options->RemoveKey(it.key());
        continue;
      }
      absl::optional<bool> stale_dns_enable_in =
          stale_dns_args->FindBoolKey(kStaleDnsEnable);
      if (stale_dns_enable_in.has_value() && stale_dns_enable_in.value()) {
        stale_dns_enable = stale_dns_enable_in.value();

        absl::optional<int> delay =
            stale_dns_args->FindIntKey(kStaleDnsDelayMs);
        if (delay)
          stale_dns_options.delay = base::Milliseconds(*delay);

        absl::optional<int> max_expired_time_ms =
            stale_dns_args->FindIntKey(kStaleDnsMaxExpiredTimeMs);
        if (max_expired_time_ms) {
          stale_dns_options.max_expired_time =
              base::Milliseconds(*max_expired_time_ms);
        }

        absl::optional<int> max_stale_uses =
            stale_dns_args->FindIntKey(kStaleDnsMaxStaleUses);
        if (max_stale_uses)
          stale_dns_options.max_stale_uses = *max_stale_uses;

        stale_dns_options.allow_other_network =
            stale_dns_args->FindBoolKey(kStaleDnsAllowOtherNetwork)
                .value_or(stale_dns_options.allow_other_network);
        enable_host_cache_persistence =
            stale_dns_args->FindBoolKey(kStaleDnsPersist)
                .value_or(enable_host_cache_persistence);

        absl::optional<int> persist_timer =
            stale_dns_args->FindIntKey(kStaleDnsPersistTimer);
        if (persist_timer)
          host_cache_persistence_delay_ms = *persist_timer;

        stale_dns_options.use_stale_on_name_not_resolved =
            stale_dns_args->FindBoolKey(kStaleDnsUseStaleOnNameNotResolved)
                .value_or(stale_dns_options.use_stale_on_name_not_resolved);
      }
    } else if (it.key() == kHostResolverRulesFieldTrialName) {
      const base::DictionaryValue* host_resolver_rules_args = nullptr;
      if (!it.value().GetAsDictionary(&host_resolver_rules_args)) {
        LOG(ERROR) << "\"" << it.key() << "\" config params \"" << it.value()
                   << "\" is not a dictionary value";
        effective_experimental_options->RemoveKey(it.key());
        continue;
      }
      host_resolver_rules_enable = host_resolver_rules_args->GetString(
          kHostResolverRules, &host_resolver_rules_string);
    } else if (it.key() == kNetworkErrorLoggingFieldTrialName) {
      const base::DictionaryValue* nel_args = nullptr;
      if (!it.value().GetAsDictionary(&nel_args)) {
        LOG(ERROR) << "\"" << it.key() << "\" config params \"" << it.value()
                   << "\" is not a dictionary value";
        effective_experimental_options->RemoveKey(it.key());
        continue;
      }
      nel_enable = nel_args->FindBoolKey(kNetworkErrorLoggingEnable)
                       .value_or(nel_enable);

      const auto* preloaded_report_to_headers_config =
          nel_args->FindKeyOfType(kNetworkErrorLoggingPreloadedReportToHeaders,
                                  base::Value::Type::LIST);
      if (preloaded_report_to_headers_config) {
        preloaded_report_to_headers = ParseNetworkErrorLoggingHeaders(
            preloaded_report_to_headers_config->GetList());
      }

      const auto* preloaded_nel_headers_config = nel_args->FindKeyOfType(
          kNetworkErrorLoggingPreloadedNELHeaders, base::Value::Type::LIST);
      if (preloaded_nel_headers_config) {
        preloaded_nel_headers = ParseNetworkErrorLoggingHeaders(
            preloaded_nel_headers_config->GetList());
      }
    } else if (it.key() == kDisableIPv6OnWifi) {
      if (!it.value().is_bool()) {
        LOG(ERROR) << "\"" << it.key() << "\" config params \"" << it.value()
                   << "\" is not a bool";
        effective_experimental_options->RemoveKey(it.key());
        continue;
      }
      disable_ipv6_on_wifi = it.value().GetBool();
    } else if (it.key() == kSSLKeyLogFile) {
      if (it.value().is_string()) {
        base::FilePath ssl_key_log_file(
            base::FilePath::FromUTF8Unsafe(it.value().GetString()));
        if (!ssl_key_log_file.empty()) {
          // SetSSLKeyLogger is only safe to call before any SSLClientSockets
          // are created. This should not be used if there are multiple
          // CronetEngine.
          // TODO(xunjieli): Expose this as a stable API after crbug.com/458365
          // is resolved.
          net::SSLClientSocket::SetSSLKeyLogger(
              std::make_unique<net::SSLKeyLoggerImpl>(ssl_key_log_file));
        }
      }
    } else if (it.key() == kNetworkQualityEstimatorFieldTrialName) {
      const base::DictionaryValue* nqe_args = nullptr;
      if (!it.value().GetAsDictionary(&nqe_args)) {
        LOG(ERROR) << "\"" << it.key() << "\" config params \"" << it.value()
                   << "\" is not a dictionary value";
        effective_experimental_options->RemoveKey(it.key());
        continue;
      }

      std::string nqe_option;
      if (nqe_args->GetString(net::kForceEffectiveConnectionType,
                              &nqe_option)) {
        nqe_forced_effective_connection_type =
            net::GetEffectiveConnectionTypeForName(nqe_option);
        if (!nqe_option.empty() && !nqe_forced_effective_connection_type) {
          LOG(ERROR) << "\"" << nqe_option
                     << "\" is not a valid effective connection type value";
        }
      }
    } else if (it.key() == kSpdyGoAwayOnIpChange) {
      if (!it.value().is_bool()) {
        LOG(ERROR) << "\"" << it.key() << "\" config params \"" << it.value()
                   << "\" is not a bool";
        effective_experimental_options->RemoveKey(it.key());
        continue;
      }
      session_params->spdy_go_away_on_ip_change = it.value().GetBool();
    } else {
      LOG(WARNING) << "Unrecognized Cronet experimental option \"" << it.key()
                   << "\" with params \"" << it.value();
      effective_experimental_options->RemoveKey(it.key());
    }
  }

  if (async_dns_enable || stale_dns_enable || host_resolver_rules_enable ||
      disable_ipv6_on_wifi) {
    std::unique_ptr<net::HostResolver> host_resolver;
    net::HostResolver::ManagerOptions host_resolver_manager_options;
    host_resolver_manager_options.insecure_dns_client_enabled =
        async_dns_enable;
    host_resolver_manager_options.check_ipv6_on_wifi = !disable_ipv6_on_wifi;
    // TODO(crbug.com/934402): Consider using a shared HostResolverManager for
    // Cronet HostResolvers.
    if (stale_dns_enable) {
      DCHECK(!disable_ipv6_on_wifi);
      host_resolver = std::make_unique<StaleHostResolver>(
          net::HostResolver::CreateStandaloneContextResolver(
              net::NetLog::Get(), std::move(host_resolver_manager_options)),
          stale_dns_options);
    } else {
      host_resolver = net::HostResolver::CreateStandaloneResolver(
          net::NetLog::Get(), std::move(host_resolver_manager_options));
    }
    if (host_resolver_rules_enable) {
      std::unique_ptr<net::MappedHostResolver> remapped_resolver(
          new net::MappedHostResolver(std::move(host_resolver)));
      remapped_resolver->SetRulesFromString(host_resolver_rules_string);
      host_resolver = std::move(remapped_resolver);
    }
    context_builder->set_host_resolver(std::move(host_resolver));
  }

#if BUILDFLAG(ENABLE_REPORTING)
  if (nel_enable) {
    auto policy = net::ReportingPolicy::Create();

    // Apps (like Cronet embedders) are generally allowed to run in the
    // background, even across network changes, so use more relaxed privacy
    // settings than when Reporting is running in the browser.
    policy->persist_reports_across_restarts = true;
    policy->persist_clients_across_restarts = true;
    policy->persist_reports_across_network_changes = true;
    policy->persist_clients_across_network_changes = true;

    context_builder->set_reporting_policy(std::move(policy));
    context_builder->set_network_error_logging_enabled(true);
  }
#endif  // BUILDFLAG(ENABLE_REPORTING)
  return true;
}

void URLRequestContextConfig::ConfigureURLRequestContextBuilder(
    net::URLRequestContextBuilder* context_builder) {
  std::string config_cache;
  if (http_cache != DISABLED) {
    net::URLRequestContextBuilder::HttpCacheParams cache_params;
    if (http_cache == DISK && !storage_path.empty()) {
      cache_params.type = net::URLRequestContextBuilder::HttpCacheParams::DISK;
      cache_params.path = base::FilePath::FromUTF8Unsafe(storage_path)
                              .Append(kDiskCacheDirectoryName);
    } else {
      cache_params.type =
          net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;
    }
    cache_params.max_size = http_cache_max_size;
    context_builder->EnableHttpCache(cache_params);
  } else {
    context_builder->DisableHttpCache();
  }
  context_builder->set_accept_language(accept_language);
  context_builder->set_user_agent(user_agent);
  net::HttpNetworkSessionParams session_params;
  session_params.enable_http2 = enable_spdy;
  session_params.enable_quic = enable_quic;
  auto quic_context = std::make_unique<net::QuicContext>();
  if (enable_quic) {
    quic_context->params()->user_agent_id = quic_user_agent_id;
    // Note goaway sessions on ip change will be turned on by default
    // for iOS unless overrided via experiemental options.
    quic_context->params()->goaway_sessions_on_ip_change =
        kDefaultQuicGoAwaySessionsOnIpChange;
  }

  // Somewhat hacky DCHECK use here. We don't want to crash third party
  // applications on invalid experimental options in prod, yet, detecting
  // an issue early is useful during development.
  bool experimental_options_success = ParseAndSetExperimentalOptions(
      context_builder, &session_params, quic_context->params());
  DCHECK(experimental_options_success);

  context_builder->set_http_network_session_params(session_params);
  context_builder->set_quic_context(std::move(quic_context));

  if (mock_cert_verifier)
    context_builder->SetCertVerifier(std::move(mock_cert_verifier));
  // Certificate Transparency is intentionally ignored in Cronet.
  // See //net/docs/certificate-transparency.md for more details.
  context_builder->set_ct_policy_enforcer(
      std::make_unique<net::DefaultCTPolicyEnforcer>());
  // TODO(mef): Use |config| to set cookies.
}

URLRequestContextConfigBuilder::URLRequestContextConfigBuilder() {}
URLRequestContextConfigBuilder::~URLRequestContextConfigBuilder() {}

std::unique_ptr<URLRequestContextConfig>
URLRequestContextConfigBuilder::Build() {
  return std::make_unique<URLRequestContextConfig>(
      enable_quic, quic_user_agent_id, enable_spdy, enable_brotli, http_cache,
      http_cache_max_size, load_disable_cache, storage_path, accept_language,
      user_agent, experimental_options, std::move(mock_cert_verifier),
      enable_network_quality_estimator,
      bypass_public_key_pinning_for_local_trust_anchors,
      network_thread_priority);
}

}  // namespace cronet
