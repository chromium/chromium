// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/url_request_context_config.h"

#include <memory>
#include <type_traits>
#include <utility>

#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/cronet/stale_host_resolver.h"
#include "net/base/address_family.h"
#include "net/cert/caching_cert_verifier.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/log/net_log.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/quic/set_quic_flag.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_key_logger_impl.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_tag.h"
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
const char kQuicMigrateSessionsEarlyV2[] = "migrate_sessions_early_v2";
const char kQuicRetryOnAlternateNetworkBeforeHandshake[] =
    "retry_on_alternate_network_before_handshake";
const char kQuicHostWhitelist[] = "host_whitelist";
const char kQuicVersion[] = "quic_version";
const char kQuicFlags[] = "set_quic_flags";
const char kRetryWithoutAltSvcOnQuicErrors[] =
    "retry_without_alt_svc_on_quic_errors";
const char kInitialDelayForBrokenAlternativeServiceSeconds[] =
    "initial_delay_for_broken_alternative_service_seconds";
const char kExponentialBackoffOnInitialDelay[] =
    "exponential_backoff_on_initial_delay";
const char kDelayMainJobWithAvailableSpdySession[] =
    "delay_main_job_with_available_spdy_session";

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

const char kAllowPortMigration[] = "allow_port_migration";

const char kDisableTlsZeroRtt[] = "disable_tls_zero_rtt";

// Whether SPDY sessions should be closed or marked as going away upon relevant
// network changes. When not specified, /net behavior varies depending on the
// underlying OS.
const char kSpdyGoAwayOnIpChange[] = "spdy_go_away_on_ip_change";

// Whether the connection status of all bidirectional streams (created through
// the Cronet engine) should be monitored.
// The value must be an integer (> 0) and will be interpreted as a suggestion
// for the period of the heartbeat signal. See
// SpdySession#EnableBrokenConnectionDetection for more info.
const char kBidiStreamDetectBrokenConnection[] =
    "bidi_stream_detect_broken_connection";

const char kUseDnsHttpsSvcbFieldTrialName[] = "UseDnsHttpsSvcb";
const char kUseDnsHttpsSvcbUseAlpn[] = "use_alpn";

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
    const base::Value::List& preloaded_headers_config) {
  std::vector<URLRequestContextConfig::PreloadedNelAndReportingHeader> result;
  for (const auto& preloaded_header_config : preloaded_headers_config) {
    if (!preloaded_header_config.is_dict())
      continue;

    const std::string* origin_config =
        preloaded_header_config.GetDict().FindString(
            kNetworkErrorLoggingOrigin);
    if (!origin_config)
      continue;
    GURL origin_url(*origin_config);
    if (!origin_url.is_valid())
      continue;
    auto origin = url::Origin::Create(origin_url);

    auto* value =
        preloaded_header_config.GetDict().Find(kNetworkErrorLoggingValue);
    if (!value)
      continue;

    result.push_back(URLRequestContextConfig::PreloadedNelAndReportingHeader(
        origin, SerializeJFVHeader(*value)));
  }
  return result;
}

// Applies |f| to the value contained by |maybe|, returns empty optional
// otherwise.
template <typename T, typename F>
auto map(std::optional<T> maybe, F&& f) {
  if (!maybe)
    return std::optional<std::invoke_result_t<F, T>>();
  return std::optional<std::invoke_result_t<F, T>>(f(maybe.value()));
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
    bool enable_spdy,
    bool enable_brotli,
    HttpCacheType http_cache,
    int http_cache_max_size,
    bool load_disable_cache,
    const std::string& storage_path,
    const std::string& accept_language,
    const std::string& user_agent,
    base::Value::Dict experimental_options,
    std::unique_ptr<net::CertVerifier> mock_cert_verifier,
    bool enable_network_quality_estimator,
    bool bypass_public_key_pinning_for_local_trust_anchors,
    std::optional<int> network_thread_priority)
    : enable_quic(enable_quic),
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
      effective_experimental_options(experimental_options.Clone()),
      experimental_options(std::move(experimental_options)),
      network_thread_priority(network_thread_priority),
      bidi_stream_detect_broken_connection(false),
      heartbeat_interval(base::Seconds(0)) {
  SetContextConfigExperimentalOptions();
}

URLRequestContextConfig::~URLRequestContextConfig() = default;

// static
std::unique_ptr<URLRequestContextConfig>
URLRequestContextConfig::CreateURLRequestContextConfig(
    bool enable_quic,
    bool enable_spdy,
    bool enable_brotli,
    HttpCacheType http_cache,
    int http_cache_max_size,
    bool load_disable_cache,
    const std::string& storage_path,
    const std::string& accept_language,
    const std::string& user_agent,
    const std::string& unparsed_experimental_options,
    std::unique_ptr<net::CertVerifier> mock_cert_verifier,
    bool enable_network_quality_estimator,
    bool bypass_public_key_pinning_for_local_trust_anchors,
    std::optional<int> network_thread_priority) {
  std::optional<base::Value::Dict> experimental_options =
      ParseExperimentalOptions(unparsed_experimental_options);
  if (!experimental_options) {
    // For the time being maintain backward compatibility by only failing to
    // parse when DCHECKs are enabled.
    if (ExperimentalOptionsParsingIsAllowedToFail())
      return nullptr;
    else
      experimental_options = base::Value::Dict();
  }
  return base::WrapUnique(new URLRequestContextConfig(
      enable_quic, enable_spdy, enable_brotli, http_cache, http_cache_max_size,
      load_disable_cache, storage_path, accept_language, user_agent,
      std::move(experimental_options).value(), std::move(mock_cert_verifier),
      enable_network_quality_estimator,
      bypass_public_key_pinning_for_local_trust_anchors,
      network_thread_priority));
}

// static
std::optional<base::Value::Dict>
URLRequestContextConfig::ParseExperimentalOptions(
    std::string unparsed_experimental_options) {
  // From a user perspective no experimental options means an empty string. The
  // underlying code instead expects and empty dictionary. Normalize this.
  if (unparsed_experimental_options.empty())
    unparsed_experimental_options = "{}";
  DVLOG(1) << "Experimental Options:" << unparsed_experimental_options;
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      unparsed_experimental_options);
  if (!parsed_json.has_value()) {
    LOG(ERROR) << "Parsing experimental options failed: '"
               << unparsed_experimental_options << "', error "
               << parsed_json.error().message;
    return std::nullopt;
  }

  base::Value::Dict* experimental_options_dict = parsed_json->GetIfDict();
  if (!experimental_options_dict) {
    LOG(ERROR) << "Experimental options string is not a dictionary: "
               << *parsed_json;
    return std::nullopt;
  }

  return std::move(*experimental_options_dict);
}

void URLRequestContextConfig::SetContextConfigExperimentalOptions() {
  const base::Value* heartbeat_interval_value =
      experimental_options.Find(kBidiStreamDetectBrokenConnection);
  if (heartbeat_interval_value) {
    if (!heartbeat_interval_value->is_int()) {
      LOG(ERROR) << "\"" << kBidiStreamDetectBrokenConnection
                 << "\" config params \"" << heartbeat_interval_value
                 << "\" is not an int";
      experimental_options.Remove(kBidiStreamDetectBrokenConnection);
      effective_experimental_options.Remove(kBidiStreamDetectBrokenConnection);
    } else {
      int heartbeat_interval_secs = heartbeat_interval_value->GetInt();
      heartbeat_interval = base::Seconds(heartbeat_interval_secs);
      bidi_stream_detect_broken_connection = heartbeat_interval_secs > 0;
      experimental_options.Remove(kBidiStreamDetectBrokenConnection);
    }
  }
}

void URLRequestContextConfig::SetContextBuilderExperimentalOptions(
    net::URLRequestContextBuilder* context_builder,
    net::HttpNetworkSessionParams* session_params,
    net::QuicParams* quic_params,
    net::handles::NetworkHandle bound_network) {
  bool async_dns_enable = false;
  bool stale_dns_enable = false;
  bool host_resolver_rules_enable = false;
  bool disable_ipv6_on_wifi = false;
  bool nel_enable = false;
  bool is_network_bound = bound_network != net::handles::kInvalidNetworkHandle;
  std::optional<net::HostResolver::HttpsSvcbOptions> https_svcb_options;

  StaleHostResolver::StaleOptions stale_dns_options;
  const std::string* host_resolver_rules_string;

  for (auto iter = experimental_options.begin();
       iter != experimental_options.end(); ++iter) {
    if (iter->first == kQuicFieldTrialName) {
      if (!iter->second.is_dict()) {
        LOG(ERROR) << "Quic config params \"" << iter->second
                   << "\" is not a dictionary value";
        effective_experimental_options.Remove(iter->first);
        continue;
      }

      const base::Value::Dict& quic_args = iter->second.GetDict();
      const std::string* quic_version_string =
          quic_args.FindString(kQuicVersion);
      if (quic_version_string) {
        quic::ParsedQuicVersionVector supported_versions =
            quic::ParseQuicVersionVectorString(*quic_version_string);
        quic::ParsedQuicVersionVector filtered_versions;
        quic::ParsedQuicVersionVector obsolete_versions =
            net::ObsoleteQuicVersions();
        for (const quic::ParsedQuicVersion& version : supported_versions) {
          if (!base::Contains(obsolete_versions, version)) {
            filtered_versions.push_back(version);
          }
        }
        if (!filtered_versions.empty()) {
          quic_params->supported_versions = filtered_versions;
        }
      }

      const std::string* quic_connection_options =
          quic_args.FindString(kQuicConnectionOptions);
      if (quic_connection_options) {
        quic_params->connection_options =
            quic::ParseQuicTagVector(*quic_connection_options);
      }

      const std::string* quic_client_connection_options =
          quic_args.FindString(kQuicClientConnectionOptions);
      if (quic_client_connection_options) {
        quic_params->client_connection_options =
            quic::ParseQuicTagVector(*quic_client_connection_options);
      }

      // TODO(rtenneti): Delete this option after apps stop using it.
      // Added this for backward compatibility.
      if (quic_args.FindBool(kQuicStoreServerConfigsInProperties)
              .value_or(false)) {
        quic_params->max_server_configs_stored_in_properties =
            net::kDefaultMaxQuicServerEntries;
      }

      quic_params->max_server_configs_stored_in_properties =
          static_cast<size_t>(
              quic_args.FindInt(kQuicMaxServerConfigsStoredInProperties)
                  .value_or(
                      quic_params->max_server_configs_stored_in_properties));

      quic_params->idle_connection_timeout =
          map(quic_args.FindInt(kQuicIdleConnectionTimeoutSeconds),
              base::Seconds<int>)
              .value_or(quic_params->idle_connection_timeout);

      quic_params->max_time_before_crypto_handshake =
          map(quic_args.FindInt(kQuicMaxTimeBeforeCryptoHandshakeSeconds),
              base::Seconds<int>)
              .value_or(quic_params->max_time_before_crypto_handshake);

      quic_params->max_idle_time_before_crypto_handshake =
          map(quic_args.FindInt(kQuicMaxIdleTimeBeforeCryptoHandshakeSeconds),
              base::Seconds<int>)
              .value_or(quic_params->max_idle_time_before_crypto_handshake);

      quic_params->close_sessions_on_ip_change =
          quic_args.FindBool(kQuicCloseSessionsOnIpChange)
              .value_or(quic_params->close_sessions_on_ip_change);

      quic_params->goaway_sessions_on_ip_change =
          quic_args.FindBool(kQuicGoAwaySessionsOnIpChange)
              .value_or(quic_params->goaway_sessions_on_ip_change);
      quic_params->allow_server_migration =
          quic_args.FindBool(kQuicAllowServerMigration)
              .value_or(quic_params->allow_server_migration);

      std::optional<bool> quic_migrate_sessions_on_network_change_v2_in =
          quic_args.FindBool(kQuicMigrateSessionsOnNetworkChangeV2);
      if (quic_migrate_sessions_on_network_change_v2_in.has_value()) {
        quic_params->migrate_sessions_on_network_change_v2 =
            quic_migrate_sessions_on_network_change_v2_in.value();
        quic_params->max_time_on_non_default_network =
            map(quic_args.FindInt(kQuicMaxTimeOnNonDefaultNetworkSeconds),
                base::Seconds<int>)
                .value_or(quic_params->max_time_on_non_default_network);
        quic_params->max_migrations_to_non_default_network_on_write_error =
            quic_args.FindInt(kQuicMaxMigrationsToNonDefaultNetworkOnWriteError)
                .value_or(
                    quic_params
                        ->max_migrations_to_non_default_network_on_write_error);
        quic_params->max_migrations_to_non_default_network_on_path_degrading =
            quic_args
                .FindInt(kQuicMaxMigrationsToNonDefaultNetworkOnPathDegrading)
                .value_or(
                    quic_params
                        ->max_migrations_to_non_default_network_on_path_degrading);
      }

      std::optional<bool> quic_migrate_idle_sessions_in =
          quic_args.FindBool(kQuicMigrateIdleSessions);
      if (quic_migrate_idle_sessions_in.has_value()) {
        quic_params->migrate_idle_sessions =
            quic_migrate_idle_sessions_in.value();
        quic_params->idle_session_migration_period =
            map(quic_args.FindInt(kQuicIdleSessionMigrationPeriodSeconds),
                base::Seconds<int>)
                .value_or(quic_params->idle_session_migration_period);
      }

      quic_params->migrate_sessions_early_v2 =
          quic_args.FindBool(kQuicMigrateSessionsEarlyV2)
              .value_or(quic_params->migrate_sessions_early_v2);

      quic_params->retransmittable_on_wire_timeout =
          map(quic_args.FindInt(kQuicRetransmittableOnWireTimeoutMilliseconds),
              base::Milliseconds<int>)
              .value_or(quic_params->retransmittable_on_wire_timeout);

      quic_params->retry_on_alternate_network_before_handshake =
          quic_args.FindBool(kQuicRetryOnAlternateNetworkBeforeHandshake)
              .value_or(
                  quic_params->retry_on_alternate_network_before_handshake);

      quic_params->allow_port_migration =
          quic_args.FindBool(kAllowPortMigration)
              .value_or(quic_params->allow_port_migration);

      quic_params->retry_without_alt_svc_on_quic_errors =
          quic_args.FindBool(kRetryWithoutAltSvcOnQuicErrors)
              .value_or(quic_params->retry_without_alt_svc_on_quic_errors);

      quic_params->initial_delay_for_broken_alternative_service = map(
          quic_args.FindInt(kInitialDelayForBrokenAlternativeServiceSeconds),
          base::Seconds<int>);

      quic_params->exponential_backoff_on_initial_delay =
          quic_args.FindBool(kExponentialBackoffOnInitialDelay);

      quic_params->delay_main_job_with_available_spdy_session =
          quic_args.FindBool(kDelayMainJobWithAvailableSpdySession)
              .value_or(
                  quic_params->delay_main_job_with_available_spdy_session);

      quic_params->disable_tls_zero_rtt =
          quic_args.FindBool(kDisableTlsZeroRtt)
              .value_or(quic_params->disable_tls_zero_rtt);

      const std::string* quic_host_allowlist =
          quic_args.FindString(kQuicHostWhitelist);
      if (quic_host_allowlist) {
        std::vector<std::string> host_vector =
            base::SplitString(*quic_host_allowlist, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_ALL);
        session_params->quic_host_allowlist.clear();
        for (const std::string& host : host_vector) {
          session_params->quic_host_allowlist.insert(host);
        }
      }

      const std::string* quic_flags = quic_args.FindString(kQuicFlags);
      if (quic_flags) {
        for (const auto& flag :
             base::SplitString(*quic_flags, ",", base::TRIM_WHITESPACE,
                               base::SPLIT_WANT_ALL)) {
          std::vector<std::string> tokens = base::SplitString(
              flag, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
          if (tokens.size() != 2)
            continue;
          net::SetQuicFlagByName(tokens[0], tokens[1]);
        }
      }
    } else if (iter->first == kAsyncDnsFieldTrialName) {
      if (!iter->second.is_dict()) {
        LOG(ERROR) << "\"" << iter->first << "\" config params \""
                   << iter->second << "\" is not a dictionary value";
        effective_experimental_options.Remove(iter->first);
        continue;
      }
      const base::Value::Dict& async_dns_args = iter->second.GetDict();
      async_dns_enable =
          async_dns_args.FindBool(kAsyncDnsEnable).value_or(async_dns_enable);
    } else if (iter->first == kStaleDnsFieldTrialName) {
      if (!iter->second.is_dict()) {
        LOG(ERROR) << "\"" << iter->first << "\" config params \""
                   << iter->second << "\" is not a dictionary value";
        effective_experimental_options.Remove(iter->first);
        continue;
      }
      const base::Value::Dict& stale_dns_args = iter->second.GetDict();
      stale_dns_enable =
          stale_dns_args.FindBool(kStaleDnsEnable).value_or(false);

      if (stale_dns_enable) {
        stale_dns_options.delay = map(stale_dns_args.FindInt(kStaleDnsDelayMs),
                                      base::Milliseconds<int>)
                                      .value_or(stale_dns_options.delay);
        stale_dns_options.max_expired_time =
            map(stale_dns_args.FindInt(kStaleDnsMaxExpiredTimeMs),
                base::Milliseconds<int>)
                .value_or(stale_dns_options.max_expired_time);
        stale_dns_options.max_stale_uses =
            stale_dns_args.FindInt(kStaleDnsMaxStaleUses)
                .value_or(stale_dns_options.max_stale_uses);
        stale_dns_options.allow_other_network =
            stale_dns_args.FindBool(kStaleDnsAllowOtherNetwork)
                .value_or(stale_dns_options.allow_other_network);
        enable_host_cache_persistence =
            stale_dns_args.FindBool(kStaleDnsPersist)
                .value_or(enable_host_cache_persistence);
        host_cache_persistence_delay_ms =
            stale_dns_args.FindInt(kStaleDnsPersistTimer)
                .value_or(host_cache_persistence_delay_ms);
        stale_dns_options.use_stale_on_name_not_resolved =
            stale_dns_args.FindBool(kStaleDnsUseStaleOnNameNotResolved)
                .value_or(stale_dns_options.use_stale_on_name_not_resolved);
      }
    } else if (iter->first == kHostResolverRulesFieldTrialName) {
      if (!iter->second.is_dict()) {
        LOG(ERROR) << "\"" << iter->first << "\" config params \""
                   << iter->second << "\" is not a dictionary value";
        effective_experimental_options.Remove(iter->first);
        continue;
      }
      const base::Value::Dict& host_resolver_rules_args =
          iter->second.GetDict();
      host_resolver_rules_string =
          host_resolver_rules_args.FindString(kHostResolverRules);
      host_resolver_rules_enable = !!host_resolver_rules_string;
    } else if (iter->first == kUseDnsHttpsSvcbFieldTrialName) {
      if (!iter->second.is_dict()) {
        LOG(ERROR) << "\"" << iter->first << "\" config params \""
                   << iter->second << "\" is not a dictionary value";
        effective_experimental_options.Remove(iter->first);
        continue;
      }
      const base::Value::Dict& args = iter->second.GetDict();
      https_svcb_options = net::HostResolver::HttpsSvcbOptions::FromDict(args);
      session_params->use_dns_https_svcb_alpn =
          args.FindBool(kUseDnsHttpsSvcbUseAlpn)
              .value_or(session_params->use_dns_https_svcb_alpn);
    } else if (iter->first == kNetworkErrorLoggingFieldTrialName) {
      if (!iter->second.is_dict()) {
        LOG(ERROR) << "\"" << iter->first << "\" config params \""
                   << iter->second << "\" is not a dictionary value";
        effective_experimental_options.Remove(iter->first);
        continue;
      }
      const base::Value::Dict& nel_args = iter->second.GetDict();
      nel_enable =
          nel_args.FindBool(kNetworkErrorLoggingEnable).value_or(nel_enable);

      const auto* preloaded_report_to_headers_config =
          nel_args.FindList(kNetworkErrorLoggingPreloadedReportToHeaders);
      if (preloaded_report_to_headers_config) {
        preloaded_report_to_headers = ParseNetworkErrorLoggingHeaders(
            *preloaded_report_to_headers_config);
      }

      const auto* preloaded_nel_headers_config =
          nel_args.FindList(kNetworkErrorLoggingPreloadedNELHeaders);
      if (preloaded_nel_headers_config) {
        preloaded_nel_headers =
            ParseNetworkErrorLoggingHeaders(*preloaded_nel_headers_config);
      }
    } else if (iter->first == kDisableIPv6OnWifi) {
      if (!iter->second.is_bool()) {
        LOG(ERROR) << "\"" << iter->first << "\" config params \""
                   << iter->second << "\" is not a bool";
        effective_experimental_options.Remove(iter->first);
        continue;
      }
      disable_ipv6_on_wifi = iter->second.GetBool();
    } else if (iter->first == kSSLKeyLogFile) {
      if (iter->second.is_string()) {
        base::FilePath ssl_key_log_file(
            base::FilePath::FromUTF8Unsafe(iter->second.GetString()));
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
    } else if (iter->first == kNetworkQualityEstimatorFieldTrialName) {
      if (!iter->second.is_dict()) {
        LOG(ERROR) << "\"" << iter->first << "\" config params \""
                   << iter->second << "\" is not a dictionary value";
        effective_experimental_options.Remove(iter->first);
        continue;
      }

      const base::Value::Dict& nqe_args = iter->second.GetDict();
      const std::string* nqe_option =
          nqe_args.FindString(net::kForceEffectiveConnectionType);
      if (nqe_option) {
        nqe_forced_effective_connection_type =
            net::GetEffectiveConnectionTypeForName(*nqe_option);
        if (!nqe_option->empty() && !nqe_forced_effective_connection_type) {
          LOG(ERROR) << "\"" << nqe_option
                     << "\" is not a valid effective connection type value";
        }
      }
    } else if (iter->first == kSpdyGoAwayOnIpChange) {
      if (!iter->second.is_bool()) {
        LOG(ERROR) << "\"" << iter->first << "\" config params \""
                   << iter->second << "\" is not a bool";
        effective_experimental_options.Remove(iter->first);
        continue;
      }
      session_params->spdy_go_away_on_ip_change = iter->second.GetBool();
    } else {
      LOG(WARNING) << "Unrecognized Cronet experimental option \""
                   << iter->first << "\" with params \"" << iter->second;
      effective_experimental_options.Remove(iter->first);
    }
  }

  if (async_dns_enable || stale_dns_enable || host_resolver_rules_enable ||
      disable_ipv6_on_wifi || is_network_bound || https_svcb_options) {
    net::HostResolver::ManagerOptions host_resolver_manager_options;
    host_resolver_manager_options.insecure_dns_client_enabled =
        async_dns_enable;
    host_resolver_manager_options.check_ipv6_on_wifi = !disable_ipv6_on_wifi;
    if (https_svcb_options) {
      host_resolver_manager_options.https_svcb_options = https_svcb_options;
    }

    if (!is_network_bound) {
      std::unique_ptr<net::HostResolver> host_resolver;
      // TODO(crbug.com/40614970): Consider using a shared HostResolverManager
      // for Cronet HostResolvers.
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
        remapped_resolver->SetRulesFromString(*host_resolver_rules_string);
        host_resolver = std::move(remapped_resolver);
      }
      context_builder->set_host_resolver(std::move(host_resolver));
    } else {
      // `stale_dns_enable` and `host_resolver_rules_enable` are purposefully
      // ignored. Implementing them requires instantiating a special
      // HostResolver that wraps the real underlying resolver: that isn't
      // possible at the moment for network-bound contexts as they create a
      // special HostResolver internally and don't expose that.
      context_builder->BindToNetwork(bound_network,
                                     std::move(host_resolver_manager_options));
    }
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
}

void URLRequestContextConfig::ConfigureURLRequestContextBuilder(
    net::URLRequestContextBuilder* context_builder,
    net::handles::NetworkHandle bound_network) {
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
    quic_context->params()->goaway_sessions_on_ip_change = false;
    // Explicitly disable network-change migration on Cronet. This is tracked
    // at crbug.com/1430096.
    quic_context->params()->migrate_sessions_on_network_change_v2 = false;
  }

  SetContextBuilderExperimentalOptions(context_builder, &session_params,
                                       quic_context->params(), bound_network);

  context_builder->set_http_network_session_params(session_params);
  context_builder->set_quic_context(std::move(quic_context));

  if (mock_cert_verifier)
    context_builder->SetCertVerifier(std::move(mock_cert_verifier));
  // TODO(mef): Use |config| to set cookies.
}

URLRequestContextConfigBuilder::URLRequestContextConfigBuilder() = default;
URLRequestContextConfigBuilder::~URLRequestContextConfigBuilder() = default;

std::unique_ptr<URLRequestContextConfig>
URLRequestContextConfigBuilder::Build() {
  return URLRequestContextConfig::CreateURLRequestContextConfig(
      enable_quic, enable_spdy, enable_brotli, http_cache, http_cache_max_size,
      load_disable_cache, storage_path, accept_language, user_agent,
      experimental_options, std::move(mock_cert_verifier),
      enable_network_quality_estimator,
      bypass_public_key_pinning_for_local_trust_anchors,
      network_thread_priority);
}

}  // namespace cronet
