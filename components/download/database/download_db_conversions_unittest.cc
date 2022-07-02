// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/database/download_db_conversions.h"

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_schedule.h"
#include "components/download/public/common/download_url_parameters.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace download {

namespace {

InProgressInfo CreateInProgressInfo() {
  InProgressInfo info;
  // InProgressInfo with valid fields.
  info.current_path = base::FilePath(FILE_PATH_LITERAL("/tmp.crdownload"));
  info.target_path = base::FilePath(FILE_PATH_LITERAL("/tmp"));
  info.url_chain.emplace_back("http://foo");
  info.url_chain.emplace_back("http://foo2");
  info.referrer_url = GURL("http://foo1.com");
  info.serialized_embedder_download_data = std::string();
  info.tab_url = GURL("http://foo.com");
  info.tab_referrer_url = GURL("http://abc.com");
  info.start_time = base::Time::NowFromSystemTime().LocalMidnight();
  info.end_time = base::Time();
  info.etag = "A";
  info.last_modified = "Wed, 1 Oct 2018 07:00:00 GMT";
  info.received_bytes = 1000;
  info.mime_type = "text/html";
  info.original_mime_type = "text/html";
  info.total_bytes = 10000;
  info.state = DownloadItem::IN_PROGRESS;
  info.danger_type = DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
  info.interrupt_reason = DOWNLOAD_INTERRUPT_REASON_NONE;
  info.transient = false;
  info.paused = false;
  info.hash = "abcdefg";
  info.metered = true;
  info.received_slices.emplace_back(0, 500, false);
  info.received_slices.emplace_back(5000, 500, false);
  info.bytes_wasted = 1234;
  info.auto_resume_count = 3;
  info.fetch_error_body = true;
  info.request_headers.emplace_back(
      std::make_pair<std::string, std::string>("123", "456"));
  info.request_headers.emplace_back(
      std::make_pair<std::string, std::string>("ABC", "def"));
  info.download_schedule = absl::make_optional<DownloadSchedule>(
      false /*only_on_wifi*/, absl::nullopt);
  info.credentials_mode = ::network::mojom::CredentialsMode::kOmit;
  return info;
}

InProgressInfo CreateInProgressInfoWithRerouteInfo(
    DownloadItemRerouteInfo reroute_info) {
  InProgressInfo info = CreateInProgressInfo();
  info.reroute_info = std::move(reroute_info);
  return info;
}

DownloadInfo CreateDownloadInfo() {
  DownloadInfo info;
  info.guid = "abcdefg";
  info.id = 1234567;
  info.in_progress_info = CreateInProgressInfo();
  info.ukm_info = UkmInfo(DownloadSource::FROM_RENDERER, 100);
  return info;
}

}  // namespace

class DownloadDBConversionsTest : public testing::Test,
                                  public DownloadDBConversions {
 public:
  ~DownloadDBConversionsTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kDownloadLater);
  }

 protected:
  base::test::ScopedFeatureList* scoped_feature_list() {
    return &scoped_feature_list_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DownloadDBConversionsTest, DownloadEntry) {
  // Entry with no fields.
  DownloadEntry entry;
  EXPECT_EQ(false, entry.fetch_error_body);
  EXPECT_TRUE(entry.request_headers.empty());
  EXPECT_EQ(entry, DownloadEntryFromProto(DownloadEntryToProto(entry)));

  // Entry with guid, request origin and download source.
  entry.guid = "guid";
  entry.request_origin = "request origin";
  entry.download_source = DownloadSource::DRAG_AND_DROP;
  entry.ukm_download_id = 123;
  entry.bytes_wasted = 1234;
  entry.fetch_error_body = true;
  entry.request_headers.emplace_back(
      std::make_pair<std::string, std::string>("123", "456"));
  entry.request_headers.emplace_back(
      std::make_pair<std::string, std::string>("ABC", "def"));
  EXPECT_EQ(entry, DownloadEntryFromProto(DownloadEntryToProto(entry)));
}

TEST_F(DownloadDBConversionsTest, DownloadEntries) {
  // Entries vector with no entries.
  std::vector<DownloadEntry> entries;
  EXPECT_EQ(entries, DownloadEntriesFromProto(DownloadEntriesToProto(entries)));

  // Entries vector with one entry.
  DownloadUrlParameters::RequestHeadersType request_headers;
  entries.push_back(DownloadEntry("guid", "request origin",
                                  DownloadSource::UNKNOWN, false,
                                  request_headers, 123));
  EXPECT_EQ(entries, DownloadEntriesFromProto(DownloadEntriesToProto(entries)));

  // Entries vector with multiple entries.
  request_headers.emplace_back(
      DownloadUrlParameters::RequestHeadersNameValuePair("key", "value"));
  entries.push_back(DownloadEntry("guid2", "request origin",
                                  DownloadSource::UNKNOWN, true,
                                  request_headers, 456));
  EXPECT_EQ(entries, DownloadEntriesFromProto(DownloadEntriesToProto(entries)));
}

TEST_F(DownloadDBConversionsTest, DownloadSource) {
  DownloadSource sources[] = {
      DownloadSource::UNKNOWN,       DownloadSource::NAVIGATION,
      DownloadSource::DRAG_AND_DROP, DownloadSource::FROM_RENDERER,
      DownloadSource::EXTENSION_API, DownloadSource::EXTENSION_INSTALLER,
      DownloadSource::INTERNAL_API,  DownloadSource::WEB_CONTENTS_API,
      DownloadSource::OFFLINE_PAGE,  DownloadSource::CONTEXT_MENU,
      DownloadSource::RETRY,         DownloadSource::RETRY_FROM_BUBBLE};

  for (auto source : sources) {
    EXPECT_EQ(source, DownloadSourceFromProto(DownloadSourceToProto(source)));
  }
}

TEST_F(DownloadDBConversionsTest, HttpRequestHeaders) {
  std::pair<std::string, std::string> header;
  EXPECT_EQ(header,
            HttpRequestHeaderFromProto(HttpRequestHeaderToProto(header)));
  header = std::make_pair("123", "456");
  EXPECT_EQ(header,
            HttpRequestHeaderFromProto(HttpRequestHeaderToProto(header)));
}

TEST_F(DownloadDBConversionsTest, InProgressInfo) {
  // InProgressInfo with no fields.
  InProgressInfo info;
  EXPECT_EQ(false, info.fetch_error_body);
  EXPECT_TRUE(info.request_headers.empty());
  EXPECT_EQ(info, InProgressInfoFromProto(InProgressInfoToProto(info)));

  // InProgressInfo with valid fields.
  info = CreateInProgressInfo();
  EXPECT_EQ(info, InProgressInfoFromProto(InProgressInfoToProto(info)));

  info.range_request_from = 5;
  info.range_request_from = 10;
  EXPECT_EQ(info, InProgressInfoFromProto(InProgressInfoToProto(info)));
}

TEST_F(DownloadDBConversionsTest, RerouteInfo) {
  DownloadItemRerouteInfo reroute_info;
  reroute_info.set_service_provider(
      enterprise_connectors::FileSystemServiceProvider::BOX);
  reroute_info.mutable_box()->set_file_id("12345");

  // InProgressInfo with valid fields.
  InProgressInfo info = CreateInProgressInfoWithRerouteInfo(reroute_info);
  EXPECT_EQ(info, InProgressInfoFromProto(InProgressInfoToProto(info)));
  EXPECT_EQ(reroute_info.SerializeAsString(),
            info.reroute_info.SerializeAsString());
}

TEST_F(DownloadDBConversionsTest, UkmInfo) {
  UkmInfo info(DownloadSource::FROM_RENDERER, 100);
  EXPECT_EQ(info, UkmInfoFromProto(UkmInfoToProto(info)));
}

TEST_F(DownloadDBConversionsTest, DownloadInfo) {
  DownloadInfo info;
  EXPECT_EQ(info, DownloadInfoFromProto(DownloadInfoToProto(info)));

  info = CreateDownloadInfo();
  EXPECT_EQ(info, DownloadInfoFromProto(DownloadInfoToProto(info)));
}

TEST_F(DownloadDBConversionsTest, DownloadDBEntry) {
  DownloadDBEntry entry;
  EXPECT_EQ(entry, DownloadDBEntryFromProto(DownloadDBEntryToProto(entry)));

  entry.download_info = CreateDownloadInfo();
  EXPECT_EQ(entry, DownloadDBEntryFromProto(DownloadDBEntryToProto(entry)));
}

TEST_F(DownloadDBConversionsTest, DownloadSchedule) {
  const bool kOnlyOnWifi = true;
  DownloadSchedule download_schedule(kOnlyOnWifi, absl::nullopt /*start_time*/);
  // InProgressInfo.metered is used to set DownloadSchedule.only_on_wifi.
  auto persisted_download_schedule = DownloadScheduleFromProto(
      DownloadScheduleToProto(download_schedule), !kOnlyOnWifi);
  EXPECT_FALSE(persisted_download_schedule.only_on_wifi());
  EXPECT_TRUE(download_schedule.only_on_wifi());

  base::Time time;
  bool success = base::Time::FromUTCString("2020-06-11 15:41", &time);
  ASSERT_TRUE(success);
  download_schedule = DownloadSchedule(kOnlyOnWifi, time);
  persisted_download_schedule = DownloadScheduleFromProto(
      DownloadScheduleToProto(download_schedule), kOnlyOnWifi);
  EXPECT_EQ(persisted_download_schedule, download_schedule);
}

// Test to verify that when download later feature is disabled, download
// schedule will not be loaded.
TEST_F(DownloadDBConversionsTest, DownloadLaterDisabled) {
  scoped_feature_list()->Reset();
  scoped_feature_list()->InitAndDisableFeature(features::kDownloadLater);

  DownloadDBEntry entry;
  entry.download_info = CreateDownloadInfo();
  EXPECT_TRUE(
      entry.download_info->in_progress_info->download_schedule.has_value());

  auto new_entry = DownloadDBEntryFromProto(DownloadDBEntryToProto(entry));
  EXPECT_FALSE(
      new_entry.download_info->in_progress_info->download_schedule.has_value());
}

}  // namespace download
