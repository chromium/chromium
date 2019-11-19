// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/session_monitor.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/default_tick_clock.h"
#include "components/mirroring/service/message_dispatcher.h"
#include "components/mirroring/service/mirror_settings.h"
#include "components/mirroring/service/value_util.h"
#include "components/mirroring/service/wifi_status_monitor.h"
#include "media/cast/cast_environment.h"
#include "media/cast/test/utility/net_utility.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/ip_endpoint.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using mirroring::mojom::CastMessage;
using mirroring::mojom::SessionError;

namespace mirroring {

namespace {

constexpr int kRetentionBytes = 512 * 1024;  // 512k.

void VerifyStringValue(const base::Value& raw_value,
                       const std::string& key,
                       const std::string& expected_value) {
  std::string data;
  EXPECT_TRUE(GetString(raw_value, key, &data));
  EXPECT_EQ(expected_value, data);
}

void VerifyBoolValue(const base::Value& raw_value,
                     const std::string& key,
                     bool expected_value) {
  bool data;
  EXPECT_TRUE(GetBool(raw_value, key, &data));
  EXPECT_EQ(expected_value, data);
}

void VerifyDoubleValue(const base::Value& raw_value,
                       const std::string& key,
                       double expected_value) {
  double data;
  EXPECT_TRUE(GetDouble(raw_value, key, &data));
  EXPECT_NEAR(expected_value, data, DBL_EPSILON * 2);
}

void VerifyWifiStatus(const base::Value& raw_value,
                      double starting_snr,
                      int starting_speed,
                      int num_of_status) {
  EXPECT_TRUE(raw_value.is_dict());
  auto* found = raw_value.FindKey("tags");
  EXPECT_TRUE(found && found->is_dict());
  auto* wifi_status = found->FindKey("receiverWifiStatus");
  EXPECT_TRUE(wifi_status && wifi_status->is_list());
  base::span<const base::Value> status_list = wifi_status->GetList();
  EXPECT_EQ(num_of_status, static_cast<int>(status_list.size()));
  for (int i = 0; i < num_of_status; ++i) {
    double snr = -1;
    int32_t speed = -1;
    int32_t timestamp = 0;
    EXPECT_TRUE(GetDouble(status_list[i], "wifiSnr", &snr));
    EXPECT_EQ(starting_snr + i, snr);
    EXPECT_TRUE(GetInt(status_list[i], "wifiSpeed", &speed));
    EXPECT_EQ(starting_speed + i, speed);
    EXPECT_TRUE(GetInt(status_list[i], "timestamp", &timestamp));
  }
}

}  // namespace

class SessionMonitorTest : public mojom::CastMessageChannel,
                           public ::testing::Test {
 public:
  SessionMonitorTest()
      : receiver_address_(media::cast::test::GetFreeLocalPort().address()),
        message_dispatcher_(receiver_.BindNewPipeAndPassRemote(),
                            inbound_channel_.BindNewPipeAndPassReceiver(),
                            error_callback_.Get()) {}
  ~SessionMonitorTest() override {}

 protected:
  // mojom::CastMessageChannel implementation.
  MOCK_METHOD1(Send, void(mojom::CastMessagePtr));

  void CreateSessionMonitor(int max_bytes, std::string* expected_settings) {
    EXPECT_CALL(*this, Send(::testing::_)).Times(::testing::AtLeast(1));
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
    auto test_url_loader_factory =
        std::make_unique<network::TestURLLoaderFactory>();
    url_loader_factory_ = test_url_loader_factory.get();
    mojo::MakeSelfOwnedReceiver(
        std::move(test_url_loader_factory),
        url_loader_factory.InitWithNewPipeAndPassReceiver());
    MirrorSettings mirror_settings;
    base::Value session_tags(base::Value::Type::DICTIONARY);
    base::Value settings = mirror_settings.ToDictionaryValue();
    if (expected_settings)
      EXPECT_TRUE(base::JSONWriter::Write(settings, expected_settings));
    session_tags.SetKey("mirrorSettings", std::move(settings));
    session_tags.SetKey("receiverProductName", base::Value("ChromeCast"));
    session_tags.SetKey("shouldCaptureAudio", base::Value(true));
    session_tags.SetKey("shouldCaptureVideo", base::Value(true));
    session_monitor_ = std::make_unique<SessionMonitor>(
        max_bytes, receiver_address_, std::move(session_tags),
        std::move(url_loader_factory));
  }

  // Generates and sends |num_of_responses| WiFi status.
  void SendWifiStatus(double starting_snr,
                      int starting_speed,
                      int num_of_responses) {
    for (int i = 0; i < num_of_responses; ++i) {
      CastMessage message;
      message.message_namespace = mojom::kWebRtcNamespace;
      message.json_format_data =
          "{\"seqNum\":" +
          std::to_string(message_dispatcher_.GetNextSeqNumber()) +
          ","
          "\"type\": \"STATUS_RESPONSE\","
          "\"result\": \"ok\","
          "\"status\": {"
          "\"wifiSnr\":" +
          std::to_string(starting_snr + i) +
          ","
          "\"wifiSpeed\": [1234, 5678, 3000, " +
          std::to_string(starting_speed + i) +
          "],"
          "\"wifiFcsError\": [12, 13, 12, 12]}"  // This will be ignored.
          "}";
      inbound_channel_->Send(message.Clone());
      task_environment_.RunUntilIdle();
    }
  }

  void StartStreamingSession() {
    cast_environment_ = new media::cast::CastEnvironment(
        base::DefaultTickClock::GetInstance(),
        task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMainThreadTaskRunner());
    EXPECT_TRUE(session_monitor_);
    auto wifi_status_monitor =
        std::make_unique<WifiStatusMonitor>(&message_dispatcher_);
    session_monitor_->StartStreamingSession(
        cast_environment_, std::move(wifi_status_monitor),
        SessionMonitor::AUDIO_AND_VIDEO, false /* is_remoting */);
    task_environment_.RunUntilIdle();
  }

  void StopStreamingSession() {
    EXPECT_TRUE(session_monitor_);
    session_monitor_->StopStreamingSession();
    cast_environment_ = nullptr;
    task_environment_.RunUntilIdle();
  }

  std::vector<SessionMonitor::EventsAndStats> AssembleBundleAndVerify(
      const std::vector<int32_t>& bundle_sizes) {
    std::vector<SessionMonitor::EventsAndStats> bundles =
        session_monitor_->AssembleBundlesAndClear(bundle_sizes);
    task_environment_.RunUntilIdle();
    EXPECT_EQ(bundle_sizes.size(), bundles.size());
    for (size_t i = 0; i < bundles.size(); ++i) {
      EXPECT_FALSE(bundles[i].first.empty());
      EXPECT_FALSE(bundles[i].second.empty());
      EXPECT_LE(
          static_cast<int>(bundles[i].first.size() + bundles[i].second.size()),
          bundle_sizes[i]);
    }
    return bundles;
  }

  base::Value ReadStats(const std::string& stats_string) {
    std::unique_ptr<base::Value> stats_ptr =
        base::JSONReader::ReadDeprecated(stats_string);
    EXPECT_TRUE(stats_ptr);
    base::Value stats = base::Value::FromUniquePtrValue(std::move(stats_ptr));
    EXPECT_TRUE(stats.is_list());
    return stats;
  }

  void SendReceiverSetupInfo(const std::string& setup_info) {
    url_loader_factory_->AddResponse(
        "http://" + receiver_address_.ToString() + ":8008/setup/eureka_info",
        setup_info);
    task_environment_.RunUntilIdle();
  }

  void TakeSnapshot() {
    ASSERT_TRUE(session_monitor_);
    session_monitor_->TakeSnapshot();
    task_environment_.RunUntilIdle();
  }

  void ReportError(SessionError error) {
    ASSERT_TRUE(session_monitor_);
    session_monitor_->OnStreamingError(error);
    task_environment_.RunUntilIdle();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  const net::IPAddress receiver_address_;
  mojo::Receiver<mojom::CastMessageChannel> receiver_{this};
  mojo::Remote<mojom::CastMessageChannel> inbound_channel_;
  base::MockCallback<MessageDispatcher::ErrorCallback> error_callback_;
  MessageDispatcher message_dispatcher_;
  network::TestURLLoaderFactory* url_loader_factory_ = nullptr;
  std::unique_ptr<SessionMonitor> session_monitor_;
  scoped_refptr<media::cast::CastEnvironment> cast_environment_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SessionMonitorTest);
};

TEST_F(SessionMonitorTest, ProvidesExpectedTags) {
  std::string expected_settings;
  CreateSessionMonitor(kRetentionBytes, &expected_settings);
  StartStreamingSession();
  SendWifiStatus(34, 2000, 5);
  std::vector<int32_t> bundle_sizes({kRetentionBytes});
  std::vector<SessionMonitor::EventsAndStats> bundles =
      AssembleBundleAndVerify(bundle_sizes);

  base::Value stats = ReadStats(bundles[0].second);
  base::span<const base::Value> stats_list = stats.GetList();
  ASSERT_EQ(1u, stats_list.size());
  // Verify tags.
  EXPECT_TRUE(stats_list[0].is_dict());
  auto* found = stats_list[0].FindKey("video");
  EXPECT_TRUE(found && found->is_dict());
  found = stats_list[0].FindKey("audio");
  EXPECT_TRUE(found && found->is_dict());
  found = stats_list[0].FindKey("tags");
  EXPECT_TRUE(found && found->is_dict());
  // Verify session tags.
  VerifyStringValue(*found, "activity", "audio+video streaming");
  VerifyStringValue(*found, "receiverProductName", "ChromeCast");
  VerifyBoolValue(*found, "shouldCaptureAudio", true);
  VerifyBoolValue(*found, "shouldCaptureVideo", true);
  auto* settings = found->FindKey("mirrorSettings");
  EXPECT_TRUE(settings && settings->is_dict());
  std::string settings_string;
  EXPECT_TRUE(base::JSONWriter::Write(*settings, &settings_string));
  EXPECT_EQ(expected_settings, settings_string);
  VerifyWifiStatus(stats_list[0], 34, 2000, 5);
}

// Test for multiple streaming sessions.
TEST_F(SessionMonitorTest, MultipleSessions) {
  CreateSessionMonitor(kRetentionBytes, nullptr);
  StartStreamingSession();
  StopStreamingSession();
  // Starts the second streaming session.
  StartStreamingSession();
  StopStreamingSession();
  std::vector<int32_t> bundle_sizes({kRetentionBytes});
  std::vector<SessionMonitor::EventsAndStats> bundles =
      AssembleBundleAndVerify(bundle_sizes);
  base::Value stats = ReadStats(bundles[0].second);
  base::span<const base::Value> stats_list = stats.GetList();
  // There should be two sessions in the recorded stats.
  EXPECT_EQ(2u, stats_list.size());
}

TEST_F(SessionMonitorTest, ConfigureMaxRetentionBytes) {
  // 2500 is an estimate number of bytes for a snapshot that includes tags and
  // five WiFi status records.
  CreateSessionMonitor(2500, nullptr);
  StartStreamingSession();
  SendWifiStatus(34, 2000, 5);
  StopStreamingSession();
  StartStreamingSession();
  SendWifiStatus(54, 3000, 5);
  StopStreamingSession();
  std::vector<int32_t> bundle_sizes({kRetentionBytes});
  std::vector<SessionMonitor::EventsAndStats> bundles =
      AssembleBundleAndVerify(bundle_sizes);
  base::Value stats = ReadStats(bundles[0].second);
  base::span<const base::Value> stats_list = stats.GetList();
  // Expect to only record the second session.
  ASSERT_EQ(1u, stats_list.size());
  VerifyWifiStatus(stats_list[0], 54, 3000, 5);
}

TEST_F(SessionMonitorTest, AssembleBundlesWithVaryingSizes) {
  CreateSessionMonitor(kRetentionBytes, nullptr);
  StartStreamingSession();
  SendWifiStatus(34, 2000, 5);
  StopStreamingSession();
  StartStreamingSession();
  SendWifiStatus(54, 3000, 5);
  StopStreamingSession();
  std::vector<int32_t> bundle_sizes({2500, kRetentionBytes});
  std::vector<SessionMonitor::EventsAndStats> bundles =
      AssembleBundleAndVerify(bundle_sizes);

  // Expect the first bundle has only one session.
  base::Value stats = ReadStats(bundles[0].second);
  base::span<const base::Value> stats_list = stats.GetList();
  // Expect to only record the second session.
  ASSERT_EQ(1u, stats_list.size());
  VerifyWifiStatus(stats_list[0], 54, 3000, 5);

  // Expect the second bundle has both sessions.
  stats = ReadStats(bundles[1].second);
  base::span<const base::Value> stats_list2 = stats.GetList();
  ASSERT_EQ(2u, stats_list2.size());
  VerifyWifiStatus(stats_list2[0], 34, 2000, 5);
  VerifyWifiStatus(stats_list2[1], 54, 3000, 5);
}

TEST_F(SessionMonitorTest, ErrorTags) {
  CreateSessionMonitor(kRetentionBytes, nullptr);
  StartStreamingSession();
  TakeSnapshot();  // Take the first snapshot.
  ReportError(SessionError::VIDEO_CAPTURE_ERROR);
  ReportError(SessionError::RTP_STREAM_ERROR);
  TakeSnapshot();          // Take the second snapshot.
  StopStreamingSession();  // Take the third snapshot.

  std::vector<int32_t> bundle_sizes({kRetentionBytes});
  std::vector<SessionMonitor::EventsAndStats> bundles =
      AssembleBundleAndVerify(bundle_sizes);
  base::Value stats = ReadStats(bundles[0].second);
  base::span<const base::Value> stats_list = stats.GetList();
  // There should be three snapshots in the bundle.
  ASSERT_EQ(3u, stats_list.size());

  // The first and the third snapshots should have no error tags.
  auto* tags = stats_list[0].FindKey("tags");
  ASSERT_TRUE(tags);
  EXPECT_FALSE(tags->FindKey("streamingErrorTime"));
  EXPECT_FALSE(tags->FindKey("streamingErrorMessage"));
  tags = stats_list[2].FindKey("tags");
  ASSERT_TRUE(tags);
  EXPECT_FALSE(tags->FindKey("streamingErrorTime"));
  EXPECT_FALSE(tags->FindKey("streamingErrorMessage"));

  // The second snapshot should have the error tags. Only the first error is
  // recorded.
  tags = stats_list[1].FindKey("tags");
  ASSERT_TRUE(tags && tags->FindKey("streamingErrorTime"));
  VerifyStringValue(*tags, "streamingErrorMessage", "Video capture error");
}

TEST_F(SessionMonitorTest, ReceiverSetupInfo) {
  CreateSessionMonitor(kRetentionBytes, nullptr);
  StartStreamingSession();
  // This snapshot should have no receiver setup info tags.
  TakeSnapshot();

  const std::string receiver_setup_info =
      "{"
      "\"cast_build_revision\": \"1.26.0.1\","
      "\"connected\": true,"
      "\"ethernet_connected\": false,"
      "\"has_update\": false,"
      "\"uptime\": 13253.6 }";

  SendReceiverSetupInfo(receiver_setup_info);

  // A final snapshot is taken and should have receiver setup info tags.
  StopStreamingSession();

  std::vector<int32_t> bundle_sizes({kRetentionBytes});
  std::vector<SessionMonitor::EventsAndStats> bundles =
      AssembleBundleAndVerify(bundle_sizes);
  base::Value stats = ReadStats(bundles[0].second);
  base::span<const base::Value> stats_list = stats.GetList();
  // There should be two snapshots in the bundle.
  EXPECT_EQ(2u, stats_list.size());

  // The first snapshot should have no receiver setup info tags.
  auto* tags = stats_list[0].FindKey("tags");
  ASSERT_TRUE(tags);
  EXPECT_FALSE(tags->FindKey("receiverVersion"));
  EXPECT_FALSE(tags->FindKey("receiverConnected"));
  EXPECT_FALSE(tags->FindKey("receiverOnEthernet"));
  EXPECT_FALSE(tags->FindKey("receiverHasUpdatePending"));
  EXPECT_FALSE(tags->FindKey("receiverUptimeSeconds"));

  // The second snapshot should have the receiver setup info tags.
  tags = stats_list[1].FindKey("tags");
  ASSERT_TRUE(tags);
  VerifyStringValue(*tags, "receiverVersion", "1.26.0.1");
  VerifyBoolValue(*tags, "receiverConnected", true);
  VerifyBoolValue(*tags, "receiverOnEthernet", false);
  VerifyBoolValue(*tags, "receiverHasUpdatePending", false);
  VerifyDoubleValue(*tags, "receiverUptimeSeconds", 13253.6);
}

}  // namespace mirroring
