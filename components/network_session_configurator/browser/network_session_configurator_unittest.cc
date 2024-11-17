// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_session_configurator/browser/network_session_configurator.h"

#include <map>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/network_session_configurator/common/network_features.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/features.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/host_port_pair.h"
#include "net/disk_cache/backend_experiment.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_factory.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/url_request/url_request_context_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network_session_configurator {

class NetworkSessionConfiguratorTest : public testing::Test {
 public:
  NetworkSessionConfiguratorTest() {
    scoped_feature_list_.Init();
    variations::testing::ClearAllVariationParams();
  }

  void ParseCommandLineAndFieldTrials(const base::CommandLine& command_line) {
    network_session_configurator::ParseCommandLineAndFieldTrials(
        command_line,
        /*is_quic_force_disabled=*/false, &params_, &quic_params_);
  }

  void ParseFieldTrials() {
    ParseCommandLineAndFieldTrials(
        base::CommandLine(base::CommandLine::NO_PROGRAM));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  net::HttpNetworkSessionParams params_;
  net::QuicParams quic_params_;
};

TEST_F(NetworkSessionConfiguratorTest, Defaults) {
  ParseFieldTrials();

  EXPECT_FALSE(params_.ignore_certificate_errors);
  EXPECT_EQ(0u, params_.testing_fixed_http_port);
  EXPECT_EQ(0u, params_.testing_fixed_https_port);
  EXPECT_FALSE(params_.enable_user_alternate_protocol_ports);

  EXPECT_TRUE(params_.enable_http2);
  EXPECT_TRUE(params_.http2_settings.empty());
  EXPECT_FALSE(params_.enable_http2_settings_grease);
  EXPECT_FALSE(params_.greased_http2_frame);
  EXPECT_FALSE(params_.http2_end_stream_with_data_frame);

  EXPECT_TRUE(params_.enable_quic);
  EXPECT_TRUE(quic_params_.retry_without_alt_svc_on_quic_errors);
  EXPECT_EQ(1250u, quic_params_.max_packet_length);
  EXPECT_EQ(quic::QuicTagVector(), quic_params_.connection_options);
  EXPECT_EQ(quic::QuicTagVector(), quic_params_.client_connection_options);
  EXPECT_FALSE(quic_params_.close_sessions_on_ip_change);
  EXPECT_FALSE(quic_params_.goaway_sessions_on_ip_change);
  EXPECT_EQ(net::kIdleConnectionTimeout, quic_params_.idle_connection_timeout);
  EXPECT_EQ(base::Seconds(quic::kPingTimeoutSecs),
            quic_params_.reduced_ping_timeout);
  EXPECT_EQ(base::Seconds(quic::kMaxTimeForCryptoHandshakeSecs),
            quic_params_.max_time_before_crypto_handshake);
  EXPECT_EQ(base::Seconds(quic::kInitialIdleTimeoutSecs),
            quic_params_.max_idle_time_before_crypto_handshake);
  EXPECT_FALSE(quic_params_.estimate_initial_rtt);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(quic_params_.migrate_sessions_on_network_change_v2);
#else  // !BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(quic_params_.migrate_sessions_on_network_change_v2);
#endif
  EXPECT_FALSE(quic_params_.migrate_sessions_early_v2);
  EXPECT_FALSE(quic_params_.retry_on_alternate_network_before_handshake);
  EXPECT_FALSE(quic_params_.migrate_idle_sessions);
  EXPECT_TRUE(quic_params_.initial_rtt_for_handshake.is_zero());
  EXPECT_TRUE(quic_params_.allow_server_migration);
  EXPECT_TRUE(params_.quic_host_allowlist.empty());
  EXPECT_TRUE(quic_params_.retransmittable_on_wire_timeout.is_zero());
  EXPECT_FALSE(quic_params_.disable_tls_zero_rtt);
  EXPECT_TRUE(quic_params_.allow_port_migration);
  EXPECT_EQ(0, quic_params_.multi_port_probing_interval);

  EXPECT_EQ(net::DefaultSupportedQuicVersions(),
            quic_params_.supported_versions);
  EXPECT_EQ(0u, quic_params_.origins_to_force_quic_on.size());
  EXPECT_FALSE(
      quic_params_.initial_delay_for_broken_alternative_service.has_value());
  EXPECT_FALSE(quic_params_.exponential_backoff_on_initial_delay.has_value());
  EXPECT_FALSE(quic_params_.delay_main_job_with_available_spdy_session);
  EXPECT_FALSE(quic_params_.use_new_alps_codepoint);
  EXPECT_FALSE(quic_params_.report_ecn);
  EXPECT_TRUE(quic_params_.enable_origin_frame);
  EXPECT_TRUE(quic_params_.skip_dns_with_origin_frame);
  EXPECT_FALSE(quic_params_.ignore_ip_matching_when_finding_existing_sessions);
}

TEST_F(NetworkSessionConfiguratorTest, Http2FieldTrialGroupNameDoesNotMatter) {
  base::FieldTrialList::CreateFieldTrial("HTTP2", "Disable");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_http2);
}

TEST_F(NetworkSessionConfiguratorTest, Http2FieldTrialDisable) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["http2_enabled"] = "false";
  base::AssociateFieldTrialParams("HTTP2", "Experiment", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("HTTP2", "Experiment");

  ParseFieldTrials();

  EXPECT_FALSE(params_.enable_http2);
}

TEST_F(NetworkSessionConfiguratorTest, DisableQuicFromFieldTrialGroup) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["enable_quic"] = "false";
  base::AssociateFieldTrialParams("QUIC", "Disabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Disabled");

  ParseFieldTrials();

  EXPECT_FALSE(params_.enable_quic);
}

TEST_F(NetworkSessionConfiguratorTest, EnableQuicFromParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["enable_quic"] = "true";
  base::AssociateFieldTrialParams("QUIC", "UseQuic", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "UseQuic");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_quic);
}

TEST_F(NetworkSessionConfiguratorTest, ValidQuicParams) {
  quic::ParsedQuicVersion version = quic::AllSupportedVersions().front();
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["enable_quic"] = "true";
  field_trial_params["channel"] = "T";
  field_trial_params["epoch"] = "90001234";  // Epoch in the far future.
  field_trial_params["quic_version"] = quic::ParsedQuicVersionToString(version);
  base::AssociateFieldTrialParams("QUIC", "ValidQuicParams",
                                  field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "ValidQuicParams");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_quic);
  EXPECT_EQ(quic_params_.supported_versions,
            quic::ParsedQuicVersionVector{version});
  EXPECT_NE(quic_params_.supported_versions,
            net::DefaultSupportedQuicVersions());
}

TEST_F(NetworkSessionConfiguratorTest, InvalidQuicParams) {
  quic::ParsedQuicVersion version = quic::AllSupportedVersions().front();
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["enable_quic"] = "true";
  // These params are missing channel and epoch.
  field_trial_params["quic_version"] = quic::ParsedQuicVersionToString(version);
  base::AssociateFieldTrialParams("QUIC", "InvalidQuicParams",
                                  field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "InvalidQuicParams");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_quic);
  EXPECT_EQ(quic_params_.supported_versions,
            net::DefaultSupportedQuicVersions());
  EXPECT_NE(quic_params_.supported_versions,
            quic::ParsedQuicVersionVector{version});
}

TEST_F(NetworkSessionConfiguratorTest, EnableQuicForDataReductionProxy) {
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  base::FieldTrialList::CreateFieldTrial("DataReductionProxyUseQuic",
                                         "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_quic);
}

TEST_F(NetworkSessionConfiguratorTest, DisableRetryWithoutAltSvcOnQuicErrors) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["retry_without_alt_svc_on_quic_errors"] = "false";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_FALSE(quic_params_.retry_without_alt_svc_on_quic_errors);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicCloseSessionsOnIpChangeFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["close_sessions_on_ip_change"] = "true";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.close_sessions_on_ip_change);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicGoAwaySessionsOnIpChangeFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["goaway_sessions_on_ip_change"] = "true";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.goaway_sessions_on_ip_change);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicRetransmittableOnWireTimeoutMillisecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["retransmittable_on_wire_timeout_milliseconds"] = "1000";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(base::Milliseconds(1000),
            quic_params_.retransmittable_on_wire_timeout);
}

TEST_F(NetworkSessionConfiguratorTest,
       InitialDelayForBrokenAlternativeServiceSeconds) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["initial_delay_for_broken_alternative_service_seconds"] =
      "5";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  ASSERT_TRUE(
      quic_params_.initial_delay_for_broken_alternative_service.has_value());
  EXPECT_EQ(base::Seconds(5),
            quic_params_.initial_delay_for_broken_alternative_service.value());
}

TEST_F(NetworkSessionConfiguratorTest, ExponentialBackOffOnInitialDelay) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["exponential_backoff_on_initial_delay"] = "true";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  ASSERT_TRUE(quic_params_.exponential_backoff_on_initial_delay.has_value());
  EXPECT_TRUE(quic_params_.exponential_backoff_on_initial_delay.value());
}

TEST_F(NetworkSessionConfiguratorTest,
       DisableExponentialBackOffOnInitialDelay) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["exponential_backoff_on_initial_delay"] = "false";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  ASSERT_TRUE(quic_params_.exponential_backoff_on_initial_delay.has_value());
  EXPECT_FALSE(quic_params_.exponential_backoff_on_initial_delay.value());
}

TEST_F(NetworkSessionConfiguratorTest, DelayMainJobWithAvailableSpdySession) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["delay_main_job_with_available_spdy_session"] = "true";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.delay_main_job_with_available_spdy_session);
}

TEST_F(NetworkSessionConfiguratorTest,
       NotDelayMainJobWithAvailableSpdySession) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["delay_main_job_with_available_spdy_session"] = "false";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_FALSE(quic_params_.delay_main_job_with_available_spdy_session);
}

TEST_F(NetworkSessionConfiguratorTest, DonotEnableOriginFrame) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["enable_origin_frame"] = "false";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_FALSE(quic_params_.enable_origin_frame);
}

TEST_F(NetworkSessionConfiguratorTest, DonotSkipDnsWithOriginFrame) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["skip_dns_with_origin_frame"] = "false";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_FALSE(quic_params_.skip_dns_with_origin_frame);
}

TEST_F(NetworkSessionConfiguratorTest,
       IgnoreIpMatchingWhenFindingExistingSessions) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["ignore_ip_matching_when_finding_existing_sessions"] =
      "true";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.ignore_ip_matching_when_finding_existing_sessions);
}

TEST_F(NetworkSessionConfiguratorTest,
       DonotIgnoreIpMatchingWhenFindingExistingSessions) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["ignore_ip_matching_when_finding_existing_sessions"] =
      "false";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_FALSE(quic_params_.ignore_ip_matching_when_finding_existing_sessions);
}

TEST_F(NetworkSessionConfiguratorTest, DonotAllowServerMigration) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["allow_server_migration"] = "false";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_FALSE(quic_params_.allow_server_migration);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicIdleConnectionTimeoutSecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["idle_connection_timeout_seconds"] = "300";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(base::Seconds(300), quic_params_.idle_connection_timeout);
}

TEST_F(NetworkSessionConfiguratorTest,
       NegativeQuicReducedPingTimeoutSecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["reduced_ping_timeout_seconds"] = "-5";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(base::Seconds(quic::kPingTimeoutSecs),
            quic_params_.reduced_ping_timeout);
}

TEST_F(NetworkSessionConfiguratorTest,
       LargeQuicReducedPingTimeoutSecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["reduced_ping_timeout_seconds"] = "50";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(base::Seconds(quic::kPingTimeoutSecs),
            quic_params_.reduced_ping_timeout);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicReducedPingTimeoutSecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["reduced_ping_timeout_seconds"] = "10";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(base::Seconds(10), quic_params_.reduced_ping_timeout);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMaxTimeBeforeCryptoHandshakeSeconds) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_time_before_crypto_handshake_seconds"] = "7";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(base::Seconds(7), quic_params_.max_time_before_crypto_handshake);
}

TEST_F(NetworkSessionConfiguratorTest,
       NegativeQuicMaxTimeBeforeCryptoHandshakeSeconds) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_time_before_crypto_handshake_seconds"] = "-1";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(base::Seconds(quic::kMaxTimeForCryptoHandshakeSecs),
            quic_params_.max_time_before_crypto_handshake);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMaxIdleTimeBeforeCryptoHandshakeSeconds) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_idle_time_before_crypto_handshake_seconds"] = "11";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(base::Seconds(11),
            quic_params_.max_idle_time_before_crypto_handshake);
}

TEST_F(NetworkSessionConfiguratorTest,
       NegativeQuicMaxIdleTimeBeforeCryptoHandshakeSeconds) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_idle_time_before_crypto_handshake_seconds"] = "-1";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(base::Seconds(quic::kInitialIdleTimeoutSecs),
            quic_params_.max_idle_time_before_crypto_handshake);
}

TEST_F(NetworkSessionConfiguratorTest, QuicEstimateInitialRtt) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["estimate_initial_rtt"] = "true";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.estimate_initial_rtt);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMigrateSessionsOnNetworkChangeV2FromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["migrate_sessions_on_network_change_v2"] = "true";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.migrate_sessions_on_network_change_v2);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMigrateSessionsEarlyV2FromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["migrate_sessions_early_v2"] = "true";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.migrate_sessions_early_v2);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicRetryOnAlternateNetworkBeforeHandshakeFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["retry_on_alternate_network_before_handshake"] = "true";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.retry_on_alternate_network_before_handshake);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicIdleSessionMigrationPeriodFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["migrate_idle_sessions"] = "true";
  field_trial_params["idle_session_migration_period_seconds"] = "15";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.migrate_idle_sessions);
  EXPECT_EQ(base::Seconds(15), quic_params_.idle_session_migration_period);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMaxTimeOnNonDefaultNetworkFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_time_on_non_default_network_seconds"] = "10";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(base::Seconds(10), quic_params_.max_time_on_non_default_network);
}

TEST_F(
    NetworkSessionConfiguratorTest,
    QuicMaxNumMigrationsToNonDefaultNetworkOnWriteErrorFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_migrations_to_non_default_network_on_write_error"] =
      "3";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(3,
            quic_params_.max_migrations_to_non_default_network_on_write_error);
}

TEST_F(
    NetworkSessionConfiguratorTest,
    QuicMaxNumMigrationsToNonDefaultNetworkOnPathDegradingFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params
      ["max_migrations_to_non_default_network_on_path_degrading"] = "4";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(
      4, quic_params_.max_migrations_to_non_default_network_on_path_degrading);
}

TEST_F(NetworkSessionConfiguratorTest,
       DisableQuicAllowPortMigrationFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["allow_port_migration"] = "false";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_FALSE(quic_params_.allow_port_migration);
}

TEST_F(NetworkSessionConfiguratorTest,
       ConfigureMultiPortProbingIntervalFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["multi_port_probing_interval"] = "10";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(10, quic_params_.multi_port_probing_interval);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicDisableTlsZeroRttFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["disable_tls_zero_rtt"] = "true";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.disable_tls_zero_rtt);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicDisableGQuicZeroRttFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["disable_gquic_zero_rtt"] = "true";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.disable_gquic_zero_rtt);
}

TEST_F(NetworkSessionConfiguratorTest, PacketLengthFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_packet_length"] = "1450";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(1450u, quic_params_.max_packet_length);
}

TEST_F(NetworkSessionConfiguratorTest, QuicVersionFromFieldTrialParamsAlpn) {
  quic::ParsedQuicVersion version = net::DefaultSupportedQuicVersions().front();
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["quic_version"] = quic::ParsedQuicVersionToString(version);
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  quic::ParsedQuicVersionVector supported_versions = {version};
  EXPECT_EQ(supported_versions, quic_params_.supported_versions);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicConnectionOptionsFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["connection_options"] = "TIME,TBBR,REJ";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  quic::QuicTagVector options;
  options.push_back(quic::kTIME);
  options.push_back(quic::kTBBR);
  options.push_back(quic::kREJ);
  EXPECT_EQ(options, quic_params_.connection_options);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicClientConnectionOptionsFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["client_connection_options"] = "TBBR,1RTT";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  quic::QuicTagVector options;
  options.push_back(quic::kTBBR);
  options.push_back(quic::k1RTT);
  EXPECT_EQ(options, quic_params_.client_connection_options);
}

TEST_F(NetworkSessionConfiguratorTest, QuicHostAllowlist) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["host_whitelist"] = "www.example.org, www.example.com";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(2u, params_.quic_host_allowlist.size());
  EXPECT_TRUE(base::Contains(params_.quic_host_allowlist, "www.example.com"));
  EXPECT_TRUE(base::Contains(params_.quic_host_allowlist, "www.example.org"));
}

TEST_F(NetworkSessionConfiguratorTest, QuicHostAllowlistEmpty) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["host_whitelist"] = "";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_host_allowlist.empty());
}

TEST_F(NetworkSessionConfiguratorTest, QuicFlags) {
  FLAGS_quiche_reloadable_flag_quic_testonly_default_false = false;
  FLAGS_quiche_restart_flag_quic_testonly_default_true = true;
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["set_quic_flags"] =
      "FLAGS_quiche_reloadable_flag_quic_testonly_default_false=true,"
      "FLAGS_quiche_restart_flag_quic_testonly_default_true=false";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(FLAGS_quiche_reloadable_flag_quic_testonly_default_false);
  EXPECT_FALSE(FLAGS_quiche_restart_flag_quic_testonly_default_true);
}

TEST_F(NetworkSessionConfiguratorTest, Http2SettingsFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["http2_settings"] = "7:1234,25:5678";
  base::AssociateFieldTrialParams("HTTP2", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("HTTP2", "Enabled");

  ParseFieldTrials();

  spdy::SettingsMap expected_settings;
  expected_settings[static_cast<spdy::SpdyKnownSettingsId>(7)] = 1234;
  expected_settings[static_cast<spdy::SpdyKnownSettingsId>(25)] = 5678;
  EXPECT_EQ(expected_settings, params_.http2_settings);
}

TEST_F(NetworkSessionConfiguratorTest, ForceQuic) {
  struct {
    bool force_enabled;
    bool force_disabled;
    bool expect_quic_enabled;
  } kTests[] = {
      {true /* force_enabled */, false /* force_disabled */,
       true /* expect_quic_enabled */},
      {false /* force_enabled */, true /* force_disabled */,
       false /* expect_quic_enabled */},
      {true /* force_enabled */, true /* force_disabled */,
       false /* expect_quic_enabled */},
  };

  for (const auto& test : kTests) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    if (test.force_enabled)
      command_line.AppendSwitch(switches::kEnableQuic);
    if (test.force_disabled)
      command_line.AppendSwitch(switches::kDisableQuic);
    ParseCommandLineAndFieldTrials(command_line);
    EXPECT_EQ(test.expect_quic_enabled, params_.enable_quic);
  }
}

TEST_F(NetworkSessionConfiguratorTest, DisableHttp2) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kDisableHttp2);
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_FALSE(params_.enable_http2);
}

TEST_F(NetworkSessionConfiguratorTest, QuicConnectionOptions) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(switches::kQuicConnectionOptions,
                                 "TIMER,TBBR,REJ");
  ParseCommandLineAndFieldTrials(command_line);

  quic::QuicTagVector expected_options;
  expected_options.push_back(quic::kTIME);
  expected_options.push_back(quic::kTBBR);
  expected_options.push_back(quic::kREJ);
  EXPECT_EQ(expected_options, quic_params_.connection_options);
}

TEST_F(NetworkSessionConfiguratorTest, QuicMaxPacketLength) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(switches::kQuicMaxPacketLength, "42");
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_EQ(42u, quic_params_.max_packet_length);
}

TEST_F(NetworkSessionConfiguratorTest, OriginToForceQuicOn) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(switches::kOriginToForceQuicOn, "*");
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_EQ(1u, quic_params_.origins_to_force_quic_on.size());
  EXPECT_EQ(1u,
            quic_params_.origins_to_force_quic_on.count(net::HostPortPair()));
}

TEST_F(NetworkSessionConfiguratorTest, OriginToForceQuicOn2) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(switches::kOriginToForceQuicOn, "foo:1234");
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_EQ(1u, quic_params_.origins_to_force_quic_on.size());
  EXPECT_EQ(1u, quic_params_.origins_to_force_quic_on.count(
                    net::HostPortPair("foo", 1234)));
}

TEST_F(NetworkSessionConfiguratorTest, OriginToForceQuicOn3) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(switches::kOriginToForceQuicOn, "foo:1,bar:2");
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_EQ(2u, quic_params_.origins_to_force_quic_on.size());
  EXPECT_EQ(1u, quic_params_.origins_to_force_quic_on.count(
                    net::HostPortPair("foo", 1)));
  EXPECT_EQ(1u, quic_params_.origins_to_force_quic_on.count(
                    net::HostPortPair("bar", 2)));
}

TEST_F(NetworkSessionConfiguratorTest, EnableUserAlternateProtocolPorts) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableUserAlternateProtocolPorts);
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_TRUE(params_.enable_user_alternate_protocol_ports);
}

TEST_F(NetworkSessionConfiguratorTest, TestingFixedPorts) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kTestingFixedHttpPort, "800");
  command_line.AppendSwitchASCII(switches::kTestingFixedHttpsPort, "801");
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_EQ(800, params_.testing_fixed_http_port);
  EXPECT_EQ(801, params_.testing_fixed_https_port);
}

TEST_F(NetworkSessionConfiguratorTest, IgnoreCertificateErrors) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kIgnoreCertificateErrors);
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_TRUE(params_.ignore_certificate_errors);
}

TEST_F(NetworkSessionConfiguratorTest, HostRules) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kHostRules, "map *.com foo");
  ParseCommandLineAndFieldTrials(command_line);

  net::HostPortPair host_port_pair("spam.net", 80);
  EXPECT_FALSE(params_.host_mapping_rules.RewriteHost(&host_port_pair));
  EXPECT_EQ("spam.net", host_port_pair.host());

  host_port_pair = net::HostPortPair("spam.com", 80);
  EXPECT_TRUE(params_.host_mapping_rules.RewriteHost(&host_port_pair));
  EXPECT_EQ("foo", host_port_pair.host());
}

TEST_F(NetworkSessionConfiguratorTest, DefaultCacheBackend) {
  if constexpr (disk_cache::IsSimpleBackendEnabledByDefaultPlatform()) {
    EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE,
              ChooseCacheType());
  } else {
    EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_BLOCKFILE,
              ChooseCacheType());
  }
}

TEST_F(NetworkSessionConfiguratorTest, DiskCacheExperimentSimpleBackend) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment, {{"backend", "simple"}});
  EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE,
            ChooseCacheType());
}

TEST_F(NetworkSessionConfiguratorTest, DiskCacheExperimentBlockfileBackend) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment, {{"backend", "blockfile"}});
  if constexpr (disk_cache::IsSimpleBackendEnabledByDefaultPlatform()) {
    EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE,
              ChooseCacheType());
  } else {
    EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_BLOCKFILE,
              ChooseCacheType());
  }
}

TEST_F(NetworkSessionConfiguratorTest, Http2GreaseSettingsFromCommandLine) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableHttp2GreaseSettings);

  ParseCommandLineAndFieldTrials(command_line);

  EXPECT_TRUE(params_.enable_http2_settings_grease);
}

TEST_F(NetworkSessionConfiguratorTest, Http2GreaseSettingsFromFieldTrial) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["http2_grease_settings"] = "true";
  base::AssociateFieldTrialParams("HTTP2", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("HTTP2", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_http2_settings_grease);
}

TEST_F(NetworkSessionConfiguratorTest,
       DisableHttp2GreaseSettingsFromCommandLineOverridesFieldTrial) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kDisableHttp2GreaseSettings);

  std::map<std::string, std::string> field_trial_params;
  field_trial_params["http2_grease_settings"] = "true";
  base::AssociateFieldTrialParams("HTTP2", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("HTTP2", "Enabled");

  ParseCommandLineAndFieldTrials(command_line);

  EXPECT_FALSE(params_.enable_http2_settings_grease);
}

TEST_F(NetworkSessionConfiguratorTest, Http2GreaseFrameTypeFromCommandLine) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kHttp2GreaseFrameType);

  ParseCommandLineAndFieldTrials(command_line);

  ASSERT_TRUE(params_.greased_http2_frame);
  const uint8_t frame_type = params_.greased_http2_frame.value().type;
  EXPECT_EQ(0x0b, frame_type % 0x1f);
}

TEST_F(NetworkSessionConfiguratorTest, Http2GreaseFrameTypeFromFieldTrial) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["http2_grease_frame_type"] = "true";
  base::AssociateFieldTrialParams("HTTP2", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("HTTP2", "Enabled");

  ParseFieldTrials();

  ASSERT_TRUE(params_.greased_http2_frame);
  const uint8_t frame_type = params_.greased_http2_frame.value().type;
  EXPECT_EQ(0x0b, frame_type % 0x1f);
}

TEST_F(NetworkSessionConfiguratorTest,
       Http2EndStreamWithDataFrameFromFieldTrial) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["http2_end_stream_with_data_frame"] = "true";
  base::AssociateFieldTrialParams("HTTP2", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("HTTP2", "Enabled");

  ParseFieldTrials();

  ASSERT_TRUE(params_.http2_end_stream_with_data_frame);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicInitialRttForHandshakeFromFieldTrailParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["initial_rtt_for_handshake_milliseconds"] = "500";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(base::Milliseconds(500), quic_params_.initial_rtt_for_handshake);
}

TEST_F(NetworkSessionConfiguratorTest,
       ReportReceivedEcnFromFieldTrailParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["report_ecn"] = "true";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.report_ecn);
}

TEST_F(NetworkSessionConfiguratorTest,
       ReportReceivedEcnFromFeature) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeature(net::features::kReportEcn);
  ParseFieldTrials();
  EXPECT_TRUE(quic_params_.report_ecn);
}

class NetworkSessionConfiguratorWithQuicVersionTest
    : public NetworkSessionConfiguratorTest,
      public ::testing::WithParamInterface<quic::ParsedQuicVersion> {
 public:
  NetworkSessionConfiguratorWithQuicVersionTest() : version_(GetParam()) {}
  ~NetworkSessionConfiguratorWithQuicVersionTest() override = default;

  const quic::ParsedQuicVersion version_;
};

INSTANTIATE_TEST_SUITE_P(QuicVersion,
                         NetworkSessionConfiguratorWithQuicVersionTest,
                         ::testing::ValuesIn(quic::AllSupportedVersions()),
                         ::testing::PrintToStringParamName());

TEST_P(NetworkSessionConfiguratorWithQuicVersionTest, QuicVersion) {
  // Note that this test covers the legacy mechanism which relies on
  // QuicVersionToString. We should now be using ALPNs instead.
  if (!version_.UsesQuicCrypto()) {
    return;
  }
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(
      switches::kQuicVersion,
      quic::QuicVersionToString(version_.transport_version));
  ParseCommandLineAndFieldTrials(command_line);
  quic::ParsedQuicVersionVector expected_versions = {version_};
  EXPECT_EQ(expected_versions, quic_params_.supported_versions);
}

TEST_P(NetworkSessionConfiguratorWithQuicVersionTest, QuicVersionAlpn) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(switches::kQuicVersion,
                                 quic::ParsedQuicVersionToString(version_));
  ParseCommandLineAndFieldTrials(command_line);
  quic::ParsedQuicVersionVector expected_versions = {version_};
  EXPECT_EQ(expected_versions, quic_params_.supported_versions);
}

TEST_P(NetworkSessionConfiguratorWithQuicVersionTest,
       SameQuicVersionsFromFieldTrialParams) {
  // Note that this test covers the legacy mechanism which relies on
  // QuicVersionToString. We should now be using ALPNs instead.
  if (!version_.UsesQuicCrypto()) {
    return;
  }
  quic::ParsedQuicVersionVector obsolete_versions = net::ObsoleteQuicVersions();
  if (base::Contains(obsolete_versions, version_)) {
    // Do not test obsolete versions here as those are covered by the
    // ObsoleteQuicVersion tests.
    return;
  }
  std::string quic_versions =
      quic::QuicVersionToString(version_.transport_version) + "," +
      quic::QuicVersionToString(version_.transport_version);
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["quic_version"] = quic_versions;
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  quic::ParsedQuicVersionVector expected_versions = {version_};
  EXPECT_EQ(expected_versions, quic_params_.supported_versions);
}

TEST_P(NetworkSessionConfiguratorWithQuicVersionTest,
       SameQuicVersionsFromFieldTrialParamsAlpn) {
  quic::ParsedQuicVersionVector obsolete_versions = net::ObsoleteQuicVersions();
  if (base::Contains(obsolete_versions, version_)) {
    // Do not test obsolete versions here as those are covered by the
    // ObsoleteQuicVersion tests.
    return;
  }
  std::string quic_versions = quic::ParsedQuicVersionToString(version_) + "," +
                              quic::ParsedQuicVersionToString(version_);
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["quic_version"] = quic_versions;
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  quic::ParsedQuicVersionVector expected_versions = {version_};
  EXPECT_EQ(expected_versions, quic_params_.supported_versions);
}

TEST_P(NetworkSessionConfiguratorWithQuicVersionTest, ObsoleteQuicVersion) {
  // Test that a single obsolete version causes us to use default versions.
  quic::ParsedQuicVersionVector obsolete_versions = net::ObsoleteQuicVersions();
  if (!base::Contains(obsolete_versions, version_)) {
    // Only test obsolete versions here.
    return;
  }
  std::string quic_versions = quic::ParsedQuicVersionToString(version_);
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["quic_version"] = quic_versions;
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(net::DefaultSupportedQuicVersions(),
            quic_params_.supported_versions);
}

TEST_P(NetworkSessionConfiguratorWithQuicVersionTest,
       ObsoleteQuicVersionWithGoodVersion) {
  // Test that when using one obsolete version and a supported version, the
  // supported version is used.
  quic::ParsedQuicVersionVector obsolete_versions = net::ObsoleteQuicVersions();
  if (!base::Contains(obsolete_versions, version_)) {
    // Only test obsolete versions here.
    return;
  }
  quic::ParsedQuicVersion good_version = quic::AllSupportedVersions().front();
  std::string quic_versions = quic::ParsedQuicVersionToString(version_) + "," +
                              quic::ParsedQuicVersionToString(good_version);
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["quic_version"] = quic_versions;
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  quic::ParsedQuicVersionVector expected_versions = {good_version};
  EXPECT_EQ(expected_versions, quic_params_.supported_versions);
}

class NetworkSessionConfiguratorWithNewAlpsCodepointTest
    : public NetworkSessionConfiguratorTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  NetworkSessionConfiguratorWithNewAlpsCodepointTest()
      : use_new_alps_codepoint_feature_setting_(std::get<0>(GetParam())),
        use_new_alps_codepoint_field_trial_setting_(std::get<1>(GetParam())) {
    if (use_new_alps_codepoint_feature_setting_) {
      feature_list_.InitAndEnableFeature(
          net::features::kUseNewAlpsCodepointQUIC);
    } else {
      feature_list_.InitAndDisableFeature(
          net::features::kUseNewAlpsCodepointQUIC);
    }
  }
  ~NetworkSessionConfiguratorWithNewAlpsCodepointTest() override = default;

  bool use_new_alps_codepoint_feature_setting() {
    return use_new_alps_codepoint_feature_setting_;
  }

  bool use_new_alps_codepoint_field_trial_setting() {
    return use_new_alps_codepoint_field_trial_setting_;
  }

 private:
  const bool use_new_alps_codepoint_feature_setting_;
  const bool use_new_alps_codepoint_field_trial_setting_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(NewAlpsCodepoint,
                         NetworkSessionConfiguratorWithNewAlpsCodepointTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

TEST_P(NetworkSessionConfiguratorWithNewAlpsCodepointTest, NoFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  // If no field trail params overrides, use feature setting.
  EXPECT_EQ(use_new_alps_codepoint_feature_setting(),
            quic_params_.use_new_alps_codepoint);
}

TEST_P(NetworkSessionConfiguratorWithNewAlpsCodepointTest,
       FromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["use_new_alps_codepoint"] =
      use_new_alps_codepoint_field_trial_setting() ? "true" : "false";
  base::AssociateFieldTrialParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  // If field trail params have value, it should override the feature setting.
  EXPECT_EQ(use_new_alps_codepoint_field_trial_setting(),
            quic_params_.use_new_alps_codepoint);
}

}  // namespace network_session_configurator
