// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/url_request_context_config.h"

#include <memory>
#include <string_view>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/http_user_agent_settings.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/http/http_network_session.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_service.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

namespace cronet {

namespace {

std::string WrapJsonHeader(std::string_view value) {
  return base::StrCat({"[", value, "]"});
}

// Returns whether two JSON-encoded headers contain the same content, ignoring
// irrelevant encoding issues like whitespace and map element ordering.
bool JsonHeaderEquals(std::string_view expected, std::string_view actual) {
  return base::test::ParseJson(WrapJsonHeader(expected)) ==
         base::test::ParseJson(WrapJsonHeader(actual));
}

}  // namespace

TEST(URLRequestContextConfigTest, TestExperimentalOptionParsing) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);

  // Create JSON for experimental options.
  base::Value::Dict options;
  options.SetByDottedPath("QUIC.max_server_configs_stored_in_properties", 2);
  options.SetByDottedPath("QUIC.idle_connection_timeout_seconds", 300);
  options.SetByDottedPath("QUIC.close_sessions_on_ip_change", true);
  options.SetByDottedPath("QUIC.connection_options", "TIME,TBBR,REJ");
  options.SetByDottedPath(
      "QUIC.set_quic_flags",
      "FLAGS_quiche_reloadable_flag_quic_testonly_default_false=true,"
      "FLAGS_quiche_restart_flag_quic_testonly_default_true=false");
  options.SetByDottedPath("AsyncDNS.enable", true);
  options.SetByDottedPath("NetworkErrorLogging.enable", true);
  options.SetByDottedPath("NetworkErrorLogging.preloaded_report_to_headers",
                          base::test::ParseJson(R"json(
                  [
                    {
                      "origin": "https://test-origin/",
                      "value": {
                        "group": "test-group",
                        "max_age": 86400,
                        "endpoints": [
                          {"url": "https://test-endpoint/"},
                        ],
                      },
                    },
                    {
                      "origin": "https://test-origin-2/",
                      "value": [
                        {
                          "group": "test-group-2",
                          "max_age": 86400,
                          "endpoints": [
                            {"url": "https://test-endpoint-2/"},
                          ],
                        },
                        {
                          "group": "test-group-3",
                          "max_age": 86400,
                          "endpoints": [
                            {"url": "https://test-endpoint-3/"},
                          ],
                        },
                      ],
                    },
                    {
                      "origin": "https://value-is-missing/",
                    },
                    {
                      "value": "origin is missing",
                    },
                    {
                      "origin": 123,
                      "value": "origin is not a string",
                    },
                    {
                      "origin": "this is not a URL",
                      "value": "origin not a URL",
                    },
                  ]
                  )json"));
  options.SetByDottedPath("NetworkErrorLogging.preloaded_nel_headers",
                          base::test::ParseJson(R"json(
                  [
                    {
                      "origin": "https://test-origin/",
                      "value": {
                        "report_to": "test-group",
                        "max_age": 86400,
                      },
                    },
                  ]
                  )json"));
  options.SetByDottedPath("UnknownOption.foo", true);
  options.SetByDottedPath("HostResolverRules.host_resolver_rules",
                          "MAP * 127.0.0.1");
  // See http://crbug.com/696569.
  options.Set("disable_ipv6_on_wifi", true);
  options.Set("spdy_go_away_on_ip_change", true);
  std::string options_json;
  EXPECT_TRUE(base::JSONWriter::Write(options, &options_json));

  // Initialize QUIC flags set by the config.
  FLAGS_quiche_reloadable_flag_quic_testonly_default_false = false;
  FLAGS_quiche_restart_flag_quic_testonly_default_true = true;

  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          options_json,
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::optional<double>(42.0));

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  EXPECT_FALSE(
      config->effective_experimental_options.contains("UnknownOption"));
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::QuicParams* quic_params = context->quic_context()->params();
  // Check Quic Connection options.
  quic::QuicTagVector quic_connection_options;
  quic_connection_options.push_back(quic::kTIME);
  quic_connection_options.push_back(quic::kTBBR);
  quic_connection_options.push_back(quic::kREJ);
  EXPECT_EQ(quic_connection_options, quic_params->connection_options);

  // Check QUIC flags.
  EXPECT_TRUE(FLAGS_quiche_reloadable_flag_quic_testonly_default_false);
  EXPECT_FALSE(FLAGS_quiche_restart_flag_quic_testonly_default_true);

  // Check max_server_configs_stored_in_properties.
  EXPECT_EQ(2u, quic_params->max_server_configs_stored_in_properties);

  // Check idle_connection_timeout.
  EXPECT_EQ(300, quic_params->idle_connection_timeout.InSeconds());

  EXPECT_TRUE(quic_params->close_sessions_on_ip_change);
  EXPECT_FALSE(quic_params->goaway_sessions_on_ip_change);
  EXPECT_TRUE(quic_params->allow_server_migration);
  EXPECT_FALSE(quic_params->migrate_sessions_on_network_change_v2);
  EXPECT_FALSE(quic_params->migrate_sessions_early_v2);
  EXPECT_FALSE(quic_params->migrate_idle_sessions);
  EXPECT_FALSE(quic_params->retry_on_alternate_network_before_handshake);
  EXPECT_TRUE(quic_params->allow_port_migration);
  EXPECT_FALSE(quic_params->disable_tls_zero_rtt);
  EXPECT_TRUE(quic_params->retry_without_alt_svc_on_quic_errors);
  EXPECT_FALSE(
      quic_params->initial_delay_for_broken_alternative_service.has_value());
  EXPECT_FALSE(quic_params->exponential_backoff_on_initial_delay.has_value());
  EXPECT_FALSE(quic_params->delay_main_job_with_available_spdy_session);

#if defined(ENABLE_BUILT_IN_DNS)
  // Check AsyncDNS resolver is enabled.
  EXPECT_TRUE(context->host_resolver()->GetDnsConfigAsValue());
#endif  // defined(ENABLE_BUILT_IN_DNS)

#if BUILDFLAG(ENABLE_REPORTING)
  // Check Reporting and Network Error Logging are enabled (can be disabled at
  // build time).
  EXPECT_TRUE(context->reporting_service());
  EXPECT_TRUE(context->network_error_logging_service());
#endif  // BUILDFLAG(ENABLE_REPORTING)

  ASSERT_EQ(2u, config->preloaded_report_to_headers.size());
  EXPECT_EQ(url::Origin::CreateFromNormalizedTuple("https", "test-origin", 443),
            config->preloaded_report_to_headers[0].origin);
  EXPECT_TRUE(JsonHeaderEquals(  //
      R"json(
      {
        "group": "test-group",
        "max_age": 86400,
        "endpoints": [
          {"url": "https://test-endpoint/"},
        ],
      }
      )json",
      config->preloaded_report_to_headers[0].value));
  EXPECT_EQ(
      url::Origin::CreateFromNormalizedTuple("https", "test-origin-2", 443),
      config->preloaded_report_to_headers[1].origin);
  EXPECT_TRUE(JsonHeaderEquals(  //
      R"json(
      {
        "group": "test-group-2",
        "max_age": 86400,
        "endpoints": [
          {"url": "https://test-endpoint-2/"},
        ],
      },
      {
        "group": "test-group-3",
        "max_age": 86400,
        "endpoints": [
          {"url": "https://test-endpoint-3/"},
        ],
      }
      )json",
      config->preloaded_report_to_headers[1].value));

  ASSERT_EQ(1u, config->preloaded_nel_headers.size());
  EXPECT_EQ(url::Origin::CreateFromNormalizedTuple("https", "test-origin", 443),
            config->preloaded_nel_headers[0].origin);
  EXPECT_TRUE(JsonHeaderEquals(  //
      R"json(
      {
        "report_to": "test-group",
        "max_age": 86400,
      }
      )json",
      config->preloaded_nel_headers[0].value));

  // Check IPv6 is disabled when on wifi.
  EXPECT_FALSE(context->host_resolver()
                   ->GetManagerForTesting()
                   ->check_ipv6_on_wifi_for_testing());

  const net::HttpNetworkSessionParams* params =
      context->GetNetworkSessionParams();
  EXPECT_TRUE(params->spdy_go_away_on_ip_change);

  // All host resolution expected to be mapped to an immediately-resolvable IP.
  std::unique_ptr<net::HostResolver::ResolveHostRequest> resolve_request =
      context->host_resolver()->CreateRequest(
          net::HostPortPair("abcde", 80), net::NetworkAnonymizationKey(),
          net::NetLogWithSource(), std::nullopt);
  EXPECT_EQ(net::OK, resolve_request->Start(base::BindOnce(
                         [](int error) { NOTREACHED_IN_MIGRATION(); })));

  EXPECT_TRUE(config->network_thread_priority);
  EXPECT_EQ(42, config->network_thread_priority.value());
  EXPECT_FALSE(config->bidi_stream_detect_broken_connection);

  // When UseDnsHttpsSvcb option is not set, the value of net::features are
  // used.
  const net::HostResolver::HttpsSvcbOptions& https_svcb_options =
      context->host_resolver()
          ->GetManagerForTesting()
          ->https_svcb_options_for_testing();
  EXPECT_EQ(base::FeatureList::IsEnabled(net::features::kUseDnsHttpsSvcb),
            https_svcb_options.enable);
  EXPECT_EQ(net::features::kUseDnsHttpsSvcbInsecureExtraTimeMax.Get(),
            https_svcb_options.insecure_extra_time_max);
  EXPECT_EQ(net::features::kUseDnsHttpsSvcbInsecureExtraTimePercent.Get(),
            https_svcb_options.insecure_extra_time_percent);
  EXPECT_EQ(net::features::kUseDnsHttpsSvcbInsecureExtraTimeMin.Get(),
            https_svcb_options.insecure_extra_time_min);
  EXPECT_EQ(net::features::kUseDnsHttpsSvcbSecureExtraTimeMax.Get(),
            https_svcb_options.secure_extra_time_max);
  EXPECT_EQ(net::features::kUseDnsHttpsSvcbSecureExtraTimePercent.Get(),
            https_svcb_options.secure_extra_time_percent);
  EXPECT_EQ(net::features::kUseDnsHttpsSvcbSecureExtraTimeMin.Get(),
            https_svcb_options.secure_extra_time_min);
  EXPECT_EQ(base::FeatureList::IsEnabled(net::features::kUseDnsHttpsSvcbAlpn),
            params->use_dns_https_svcb_alpn);
}

TEST(URLRequestContextConfigTest, SetSupportedQuicVersionByAlpn) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);

  quic::ParsedQuicVersion version = quic::AllSupportedVersions().front();
  std::string experimental_options = "{\"QUIC\":{\"quic_version\":\"" +
                                     quic::ParsedQuicVersionToString(version) +
                                     "\"}}";

  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          experimental_options,
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::QuicParams* quic_params = context->quic_context()->params();
  EXPECT_EQ(quic_params->supported_versions.size(), 1u);
  EXPECT_EQ(quic_params->supported_versions[0], version);
}

TEST(URLRequestContextConfigTest, SetUnsupportedQuicVersion) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);

  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"QUIC\":{\"quic_version\":\"h3-Q047\"}}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::QuicParams* quic_params = context->quic_context()->params();
  EXPECT_EQ(quic_params->supported_versions,
            net::DefaultSupportedQuicVersions());
}

TEST(URLRequestContextConfigTest, SetObsoleteQuicVersion) {
  // This test configures cronet with an obsolete QUIC version and validates
  // that cronet ignores that version and uses the default versions.
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);

  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          std::string("{\"QUIC\":{\"quic_version\":\"") +
              quic::ParsedQuicVersionToString(
                  net::ObsoleteQuicVersions().back()) +
              "\"}}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::QuicParams* quic_params = context->quic_context()->params();
  EXPECT_EQ(quic_params->supported_versions,
            net::DefaultSupportedQuicVersions());
}

TEST(URLRequestContextConfigTest, SetQuicServerMigrationOptions) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);

  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"QUIC\":{\"allow_server_migration\":false}}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::QuicParams* quic_params = context->quic_context()->params();

  EXPECT_FALSE(quic_params->close_sessions_on_ip_change);
  EXPECT_FALSE(quic_params->allow_server_migration);
}

// Tests that goaway_sessions_on_ip_changes can be set on via
// experimental options.
TEST(URLRequestContextConfigTest,
     SetQuicGoAwaySessionsOnIPChangeViaExperimentOptions) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);

  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"QUIC\":{\"goaway_sessions_on_ip_change\":true}}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::QuicParams* quic_params = context->quic_context()->params();

  EXPECT_FALSE(quic_params->close_sessions_on_ip_change);
  EXPECT_TRUE(quic_params->goaway_sessions_on_ip_change);
}

TEST(URLRequestContextConfigTest, SetQuicConnectionMigrationV2Options) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);

  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"QUIC\":{\"migrate_sessions_on_network_change_v2\":true,"
          "\"migrate_sessions_early_v2\":true,"
          "\"retry_on_alternate_network_before_handshake\":true,"
          "\"migrate_idle_sessions\":true,"
          "\"retransmittable_on_wire_timeout_milliseconds\":1000,"
          "\"idle_session_migration_period_seconds\":15,"
          "\"max_time_on_non_default_network_seconds\":10,"
          "\"max_migrations_to_non_default_network_on_write_error\":3,"
          "\"max_migrations_to_non_default_network_on_path_degrading\":4}}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::QuicParams* quic_params = context->quic_context()->params();

  EXPECT_TRUE(quic_params->migrate_sessions_on_network_change_v2);
  EXPECT_TRUE(quic_params->migrate_sessions_early_v2);
  EXPECT_TRUE(quic_params->retry_on_alternate_network_before_handshake);
  EXPECT_EQ(1000,
            quic_params->retransmittable_on_wire_timeout.InMilliseconds());
  EXPECT_TRUE(quic_params->migrate_idle_sessions);
  EXPECT_EQ(base::Seconds(15), quic_params->idle_session_migration_period);
  EXPECT_EQ(base::Seconds(10), quic_params->max_time_on_non_default_network);
  EXPECT_EQ(3,
            quic_params->max_migrations_to_non_default_network_on_write_error);
  EXPECT_EQ(
      4, quic_params->max_migrations_to_non_default_network_on_path_degrading);
  EXPECT_EQ(net::DefaultSupportedQuicVersions(),
            quic_params->supported_versions);
}

TEST(URLRequestContextConfigTest, SetQuicAllowPortMigration) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);
  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"QUIC\":{\"allow_port_migration\":false}}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::QuicParams* quic_params = context->quic_context()->params();

  EXPECT_FALSE(quic_params->allow_port_migration);
}

TEST(URLRequestContextConfigTest, DisableQuicRetryWithoutAltSvcOnQuicErrors) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);
  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"QUIC\":{\"retry_without_alt_svc_on_quic_errors\":false}}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::QuicParams* quic_params = context->quic_context()->params();

  EXPECT_FALSE(quic_params->retry_without_alt_svc_on_quic_errors);
}

TEST(URLRequestContextConfigTest, BrokenAlternativeServiceDelayParams1) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);
  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"QUIC\":{\"initial_delay_for_broken_alternative_service_seconds\":"
          "1,"
          "\"exponential_backoff_on_initial_delay\":true}}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::QuicParams* quic_params = context->quic_context()->params();

  ASSERT_TRUE(
      quic_params->initial_delay_for_broken_alternative_service.has_value());
  EXPECT_EQ(base::Seconds(1),
            quic_params->initial_delay_for_broken_alternative_service.value());
  ASSERT_TRUE(quic_params->exponential_backoff_on_initial_delay.has_value());
  EXPECT_TRUE(quic_params->exponential_backoff_on_initial_delay.value());
}

TEST(URLRequestContextConfigTest, BrokenAlternativeServiceDelayParams2) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);
  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"QUIC\":{\"initial_delay_for_broken_alternative_service_seconds\":"
          "5,"
          "\"exponential_backoff_on_initial_delay\":false}}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::QuicParams* quic_params = context->quic_context()->params();

  ASSERT_TRUE(
      quic_params->initial_delay_for_broken_alternative_service.has_value());
  EXPECT_EQ(base::Seconds(5),
            quic_params->initial_delay_for_broken_alternative_service.value());
  ASSERT_TRUE(quic_params->exponential_backoff_on_initial_delay.has_value());
  EXPECT_FALSE(quic_params->exponential_backoff_on_initial_delay.value());
}

TEST(URLRequestContextConfigTest, DelayMainJobWithAvailableSpdySession) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);
  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"QUIC\":{\"delay_main_job_with_available_spdy_session\":true}}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::QuicParams* quic_params = context->quic_context()->params();

  EXPECT_TRUE(quic_params->delay_main_job_with_available_spdy_session);
}

TEST(URLRequestContextConfigTest, SetDisableTlsZeroRtt) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);
  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"QUIC\":{\"disable_tls_zero_rtt\":true}}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::QuicParams* quic_params = context->quic_context()->params();

  EXPECT_TRUE(quic_params->disable_tls_zero_rtt);
}

TEST(URLRequestContextConfigTest, SetQuicHostWhitelist) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);

  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"QUIC\":{\"host_whitelist\":\"www.example.com,www.example.org\"}}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::HttpNetworkSessionParams* params =
      context->GetNetworkSessionParams();

  EXPECT_TRUE(base::Contains(params->quic_host_allowlist, "www.example.com"));
  EXPECT_TRUE(base::Contains(params->quic_host_allowlist, "www.example.org"));
}

TEST(URLRequestContextConfigTest, SetQuicMaxTimeBeforeCryptoHandshake) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);

  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"QUIC\":{\"max_time_before_crypto_handshake_seconds\":7,"
          "\"max_idle_time_before_crypto_handshake_seconds\":11}}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::QuicParams* quic_params = context->quic_context()->params();

  EXPECT_EQ(7, quic_params->max_time_before_crypto_handshake.InSeconds());
  EXPECT_EQ(11, quic_params->max_idle_time_before_crypto_handshake.InSeconds());
}

TEST(URLURLRequestContextConfigTest, SetQuicConnectionOptions) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);

  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"QUIC\":{\"connection_options\":\"TIME,TBBR,REJ\","
          "\"client_connection_options\":\"TBBR,1RTT\"}}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::QuicParams* quic_params = context->quic_context()->params();

  quic::QuicTagVector connection_options;
  connection_options.push_back(quic::kTIME);
  connection_options.push_back(quic::kTBBR);
  connection_options.push_back(quic::kREJ);
  EXPECT_EQ(connection_options, quic_params->connection_options);

  quic::QuicTagVector client_connection_options;
  client_connection_options.push_back(quic::kTBBR);
  client_connection_options.push_back(quic::k1RTT);
  EXPECT_EQ(client_connection_options, quic_params->client_connection_options);
}

TEST(URLURLRequestContextConfigTest, SetAcceptLanguageAndUserAgent) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);

  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  EXPECT_EQ("foreign-language",
            context->http_user_agent_settings()->GetAcceptLanguage());
  EXPECT_EQ("fake agent", context->http_user_agent_settings()->GetUserAgent());
}

TEST(URLURLRequestContextConfigTest, TurningOffQuic) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);

  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          false,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::HttpNetworkSessionParams* params =
      context->GetNetworkSessionParams();
  EXPECT_EQ(false, params->enable_quic);
}

TEST(URLURLRequestContextConfigTest, DefaultEnableQuic) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);

  URLRequestContextConfigBuilder config_builder;
  std::unique_ptr<URLRequestContextConfig> config = config_builder.Build();
  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::HttpNetworkSessionParams* params =
      context->GetNetworkSessionParams();
  EXPECT_EQ(true, params->enable_quic);
}

TEST(URLRequestContextConfigTest, SetSpdyGoAwayOnIPChange) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);

  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"spdy_go_away_on_ip_change\":false}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());
  const net::HttpNetworkSessionParams* params =
      context->GetNetworkSessionParams();
  EXPECT_FALSE(params->spdy_go_away_on_ip_change);
}

TEST(URLRequestContextConfigTest, WrongSpdyGoAwayOnIPChangeValue) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);

  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"spdy_go_away_on_ip_change\":\"not a bool\"}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  EXPECT_FALSE(config->effective_experimental_options.contains(
      "spdy_go_away_on_ip_change"));
}

TEST(URLRequestContextConfigTest, BidiStreamDetectBrokenConnection) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);

  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"bidi_stream_detect_broken_connection\":10}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  EXPECT_TRUE(config->effective_experimental_options.contains(
      "bidi_stream_detect_broken_connection"));
  EXPECT_TRUE(config->bidi_stream_detect_broken_connection);
  EXPECT_EQ(config->heartbeat_interval, base::Seconds(10));
}

TEST(URLRequestContextConfigTest, WrongBidiStreamDetectBrokenConnectionValue) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);

  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"bidi_stream_detect_broken_connection\": \"not an int\"}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  EXPECT_FALSE(config->effective_experimental_options.contains(
      "bidi_stream_detect_broken_connection"));
}

TEST(URLRequestContextConfigTest, HttpsSvcbOptions) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);

  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"UseDnsHttpsSvcb\":{\"enable\":true,"
          "\"insecure_extra_time_max\":\"1ms\","
          "\"insecure_extra_time_percent\":2,"
          "\"insecure_extra_time_min\":\"3ms\","
          "\"secure_extra_time_max\":\"4ms\","
          "\"secure_extra_time_percent\":5,"
          "\"secure_extra_time_min\":\"6ms\","
          "\"use_alpn\":true"
          "}}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);
  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());

  const net::HostResolver::HttpsSvcbOptions& https_svcb_options =
      context->host_resolver()
          ->GetManagerForTesting()
          ->https_svcb_options_for_testing();
  EXPECT_TRUE(https_svcb_options.enable);
  EXPECT_EQ(base::Milliseconds(1), https_svcb_options.insecure_extra_time_max);
  EXPECT_EQ(2, https_svcb_options.insecure_extra_time_percent);
  EXPECT_EQ(base::Milliseconds(3), https_svcb_options.insecure_extra_time_min);
  EXPECT_EQ(base::Milliseconds(4), https_svcb_options.secure_extra_time_max);
  EXPECT_EQ(5, https_svcb_options.secure_extra_time_percent);
  EXPECT_EQ(base::Milliseconds(6), https_svcb_options.secure_extra_time_min);

  const net::HttpNetworkSessionParams* params =
      context->GetNetworkSessionParams();
  EXPECT_TRUE(params->use_dns_https_svcb_alpn);
}

// See stale_host_resolver_unittest.cc for test of StaleDNS options.

}  // namespace cronet
