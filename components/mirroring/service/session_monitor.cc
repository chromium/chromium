// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/session_monitor.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/stl_util.h"
#include "components/mirroring/service/value_util.h"
#include "components/mirroring/service/wifi_status_monitor.h"
#include "components/version_info/version_info.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/log_serializer.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/proto/raw_events.pb.h"
#include "media/cast/logging/raw_event_subscriber_bundle.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

using mirroring::mojom::SessionError;

namespace mirroring {

namespace {

// Interval between snapshots of Cast Streaming events/stats.
constexpr base::TimeDelta kSnapshotInterval =
    base::TimeDelta::FromMinutes(15);  // Typical: 15 min → ~3 MB

// The maximum number of bytes for receiver's setup info. 256kb should be more
// than sufficient.
constexpr int kMaxSetupResponseSizeBytes = 262144;

// Returns the number of milliseconds elapsed since epoch.
int32_t ToEpochTime(const base::Time& time) {
  return (time - base::Time::UnixEpoch()).InMilliseconds();
}

// Helper to parse the response for receiver setup info and update the tags.
bool ParseReceiverSetupInfo(const std::string& response,
                            base::Value* tags,
                            std::string* receiver_name) {
  DCHECK(tags);
  std::unique_ptr<base::Value> value =
      base::JSONReader::ReadDeprecated(response);

  std::string build_version;
  bool is_connected = false;
  bool is_on_ethernet = false;
  bool has_update = false;
  double uptime_seconds = 0;

  const bool result =
      value && value->is_dict() &&
      GetString(*value, "cast_build_revision", &build_version) &&
      GetBool(*value, "connected", &is_connected) &&
      GetBool(*value, "ethernet_connected", &is_on_ethernet) &&
      GetBool(*value, "has_update", &has_update) &&
      GetDouble(*value, "uptime", &uptime_seconds) &&
      GetString(*value, "name", receiver_name);
  if (result) {
    tags->SetKey("receiverVersion", base::Value(build_version));
    tags->SetKey("receiverConnected", base::Value(is_connected));
    tags->SetKey("receiverOnEthernet", base::Value(is_on_ethernet));
    tags->SetKey("receiverHasUpdatePending", base::Value(has_update));
    tags->SetKey("receiverUptimeSeconds", base::Value(uptime_seconds));
  }
  return result;
}

const char* ToErrorMessage(SessionError error) {
  switch (error) {
    case SessionError::ANSWER_TIME_OUT:
      return "ANSWER response time out";
    case SessionError::ANSWER_NOT_OK:
      return "Received an error ANSWER response";
    case SessionError::ANSWER_MISMATCHED_CAST_MODE:
      return "Unexpected cast mode in ANSWER response.";
    case SessionError::ANSWER_MISMATCHED_SSRC_LENGTH:
      return "sendIndexes.length != ssrcs.length in ANSWER";
    case SessionError::ANSWER_SELECT_MULTIPLE_AUDIO:
      return "Receiver selected audio RTP stream twice in ANSWER";
    case SessionError::ANSWER_SELECT_MULTIPLE_VIDEO:
      return "Receiver selected video RTP stream twice in ANSWER";
    case SessionError::ANSWER_SELECT_INVALID_INDEX:
      return "Invalid indexes selected in ANSWER";
    case SessionError::ANSWER_NO_AUDIO_OR_VIDEO:
      return "Incorrect ANSWER message: No audio or Video.";
    case SessionError::AUDIO_CAPTURE_ERROR:
      return "Audio capture error";
    case SessionError::VIDEO_CAPTURE_ERROR:
      return "Video capture error";
    case SessionError::RTP_STREAM_ERROR:
      return "RTP stream error";
    case SessionError::ENCODING_ERROR:
      return "Encoding status error";
    case SessionError::CAST_TRANSPORT_ERROR:
      return "Transport error";
  }
  return "";
}

}  // namespace

SessionMonitor::SessionMonitor(
    int max_retention_bytes,
    const net::IPAddress& receiver_address,
    base::Value session_tags,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> loader_factory)
    : max_retention_bytes_(max_retention_bytes),
      receiver_address_(receiver_address),
      session_tags_(std::move(session_tags)),
      url_loader_factory_(std::move(loader_factory)),
      stored_snapshots_bytes_(0) {
  QueryReceiverSetupInfo();
}

SessionMonitor::~SessionMonitor() {}

void SessionMonitor::StartStreamingSession(
    scoped_refptr<media::cast::CastEnvironment> cast_environment,
    std::unique_ptr<WifiStatusMonitor> wifi_status_monitor,
    SessionType session_type,
    bool is_remoting) {
  DCHECK(!event_subscribers_);
  DCHECK(!snapshot_timer_.IsRunning());

  wifi_status_monitor_ = std::move(wifi_status_monitor);
  std::string session_activity =
      session_type == AUDIO_AND_VIDEO
          ? "audio+video"
          : session_type == AUDIO_ONLY ? "audio-only" : "video-only";
  session_activity += is_remoting ? " remoting" : " streaming";
  session_tags_.SetKey("activity", base::Value(session_activity));

  // Query the receiver setup info at the beginning of each streaming session.
  QueryReceiverSetupInfo();

  // Start collecting Cast Streaming events/stats.
  event_subscribers_ = std::make_unique<media::cast::RawEventSubscriberBundle>(
      std::move(cast_environment));
  if (session_type != VIDEO_ONLY)
    event_subscribers_->AddEventSubscribers(true /* is_audio */);
  if (session_type != AUDIO_ONLY)
    event_subscribers_->AddEventSubscribers(false /* is_audio */);

  // Start periodically snapshotting Cast Streaming events/stats.
  snapshot_timer_.Start(FROM_HERE, kSnapshotInterval,
                        base::BindRepeating(&SessionMonitor::TakeSnapshot,
                                            base::Unretained(this)));

  start_time_ = base::Time::Now();
}

void SessionMonitor::StopStreamingSession() {
  if (snapshot_timer_.IsRunning()) {
    snapshot_timer_.Stop();
    TakeSnapshot();  // Final snapshot of this streaming session.
  }
  event_subscribers_.reset();
  wifi_status_monitor_.reset();
}

void SessionMonitor::OnStreamingError(SessionError error) {
  DVLOG(2) << error;

  if (!snapshot_timer_.IsRunning())
    return;  // Ignore errors before streaming starts.
  // If the error has already been recorded, do not overwrite it with another
  // since the first will usually be the most indicative of the problem.
  if (error_.has_value())
    return;
  error_time_ = base::Time::Now();
  error_.emplace(error);
}

std::vector<SessionMonitor::EventsAndStats>
SessionMonitor::AssembleBundlesAndClear(
    const std::vector<int32_t>& bundle_sizes) {
  std::vector<EventsAndStats> bundles;
  // If a streaming session is currently active, take a snapshot now so that all
  // data collected since the last automatic periodic snapshot is included in
  // the bundle.
  if (snapshot_timer_.IsRunning()) {
    TakeSnapshot();
    snapshot_timer_.Reset();
  }

  for (int32_t max_bytes : bundle_sizes)
    bundles.emplace_back(MakeSliceOfSnapshots(max_bytes));
  snapshots_.clear();
  stored_snapshots_bytes_ = 0;
  return bundles;
}

SessionMonitor::EventsAndStats SessionMonitor::MakeSliceOfSnapshots(
    int32_t max_bytes) {
  // Immediately subtract two bytes for array brackets ("[]") since
  // AssembleSnapshotsAndClear() will produce a JSON array of each snapshot's
  // stats JSON.
  max_bytes -= 2;
  base::circular_deque<EventsAndStats> slice;
  for (int i = snapshots_.size() - 1; i >= 0; --i) {
    max_bytes -= snapshots_[i].second.length() + 1 /* size of the comma */;
    // If insufficient bytes remain to retain the current stats JSON, stop
    // adding more Snapshots to the slice.
    if (max_bytes < 0)
      break;
    slice.emplace_front(std::make_pair("", snapshots_[i].second));
    // If sufficient bytes remain to include the current events Blob, add it to
    // the slice.
    if (!snapshots_[i].first.empty()) {
      const int32_t events_size = snapshots_[i].first.length();
      if (max_bytes >= events_size) {
        slice[0].first = snapshots_[i].first;
        max_bytes -= events_size;
      }
    }
  }

  EventsAndStats bundle;
  if (slice.empty())
    return bundle;

  bundle.second = "[";
  for (size_t i = 0; i < slice.size(); i++) {
    // To produce a single events gzipped-data Blob, simply concatenate the
    // individual gzipped-data Blobs. The spec for gzip explicitly allows for
    // this. :-)
    bundle.first += slice[i].first;
    // To produce the JSON stats array, concatenate the mix of string and Blob
    // objects to produce a single UTF-8 encoded string.
    if (i > 0)
      bundle.second += ",";
    bundle.second += slice[i].second;
  }
  bundle.second += "]";

  return bundle;
}

void SessionMonitor::TakeSnapshot() {
  // Session-level tags.
  base::Value tags = session_tags_.Clone();

  // Add snapshot-level tags.
  tags.SetKey("startTime", base::Value(ToEpochTime(start_time_)));
  const base::Time end_time = base::Time::Now();
  tags.SetKey("endTime", base::Value(ToEpochTime(end_time)));
  start_time_ = end_time;

  if (wifi_status_monitor_) {
    const std::vector<WifiStatus> wifi_status =
        wifi_status_monitor_->GetRecentValues();
    base::Value::ListStorage wifi_status_list;
    for (const auto& status : wifi_status) {
      base::Value status_value(base::Value::Type::DICTIONARY);
      status_value.SetKey("wifiSnr", base::Value(status.snr));
      status_value.SetKey("wifiSpeed", base::Value(status.speed));
      status_value.SetKey("timestamp",
                          base::Value(ToEpochTime(status.timestamp)));
      wifi_status_list.emplace_back(std::move(status_value));
    }
    tags.SetKey("receiverWifiStatus", base::Value(wifi_status_list));
  }

  // Streaming error tags (if any).
  if (error_.has_value()) {
    tags.SetKey("streamingErrorTime", base::Value(ToEpochTime(error_time_)));
    tags.SetKey("streamingErrorMessage",
                base::Value(ToErrorMessage(error_.value())));
    error_.reset();
  }

  std::string tags_string;
  base::JSONWriter::Write(tags, &tags_string);

  // Collect raw events.
  std::string events = GetEventLogsAndReset(true, tags_string) +
                       GetEventLogsAndReset(false, tags_string);

  // Collect stats.
  std::unique_ptr<base::DictionaryValue> audio_stats =
      base::DictionaryValue::From(GetStatsAndReset(true));
  std::unique_ptr<base::DictionaryValue> video_stats =
      base::DictionaryValue::From(GetStatsAndReset(false));
  base::DictionaryValue stats;
  if (audio_stats)
    stats.MergeDictionary(audio_stats.get());
  if (video_stats)
    stats.MergeDictionary(video_stats.get());
  stats.SetKey("tags", std::move(tags));
  std::string stats_string;
  base::JSONWriter::Write(stats, &stats_string);

  int snapshots_bytes =
      stored_snapshots_bytes_ + events.size() + stats_string.size();
  // Prune |snapshots_| if necessary.
  while (snapshots_bytes > max_retention_bytes_) {
    snapshots_bytes -= snapshots_[0].first.size();
    snapshots_[0].first = std::string();
    if (snapshots_bytes <= max_retention_bytes_)
      break;
    snapshots_bytes -= snapshots_[0].second.size();
    snapshots_.pop_front();
  }
  snapshots_.emplace_back(std::make_pair(events, stats_string));
  stored_snapshots_bytes_ = snapshots_bytes;
}

std::string SessionMonitor::GetReceiverBuildVersion() const {
  std::string build_version;
  GetString(session_tags_, "receiverVersion", &build_version);
  return build_version;
}

std::string SessionMonitor::GetEventLogsAndReset(
    bool is_audio,
    const std::string& extra_data) {
  std::string result;
  if (!event_subscribers_.get())
    return result;

  media::cast::EncodingEventSubscriber* subscriber =
      event_subscribers_->GetEncodingEventSubscriber(is_audio);
  if (!subscriber)
    return result;

  media::cast::proto::LogMetadata metadata;
  media::cast::FrameEventList frame_events;
  media::cast::PacketEventList packet_events;

  subscriber->GetEventsAndReset(&metadata, &frame_events, &packet_events);

  if (!extra_data.empty())
    metadata.set_extra_data(extra_data);
  media::cast::proto::GeneralDescription* gen_desc =
      metadata.mutable_general_description();
  gen_desc->set_product(version_info::GetProductName());
  gen_desc->set_product_version(version_info::GetVersionNumber());
  gen_desc->set_os(version_info::GetOSType());

  result.resize(media::cast::kMaxSerializedBytes);
  int output_bytes;
  // TODO(crbug.com/1015471): media::cast::SerializeEvents() shouldn't require
  // the caller to pre-allocate the memory. It should return a string result.
  if (media::cast::SerializeEvents(metadata, frame_events, packet_events,
                                   true /* compress */,
                                   media::cast::kMaxSerializedBytes,
                                   base::data(result), &output_bytes)) {
    result.resize(output_bytes);
  } else {
    result.clear();
  }
  return result;
}

std::unique_ptr<base::Value> SessionMonitor::GetStatsAndReset(bool is_audio) {
  if (!event_subscribers_.get())
    return nullptr;

  media::cast::StatsEventSubscriber* subscriber =
      event_subscribers_->GetStatsEventSubscriber(is_audio);
  if (!subscriber)
    return nullptr;

  std::unique_ptr<base::Value> stats = subscriber->GetStats();
  subscriber->Reset();
  return stats;
}

void SessionMonitor::QueryReceiverSetupInfo() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "GET";
  resource_request->url = GURL("http://" + receiver_address_.ToString() +
                               ":8008/setup/eureka_info");
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("mirroring_get_setup_info", R"(
          semantics {
            sender: "Mirroring Service"
            description:
              "Mirroring Service sends a request to the receiver to obtain its "
              "setup info such as the build version, the model name, etc. The "
              "data is included in mirroring feedback for analysis."
            trigger:
              "A tab/desktop mirroring session starts."
            data: "An HTTP GET request."
            destination: OTHER
            destination_other:
              "A mirroring receiver, such as a ChromeCast device."
          }
          policy {
            cookies_allowed: NO
            setting: "This feature cannot be disabled in settings."
            chrome_policy {
              EnableMediaRouter {
                EnableMediaRouter: false
              }
            }
          })");
  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  network::SimpleURLLoader* url_loader_ptr = url_loader.get();
  url_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          [](base::WeakPtr<SessionMonitor> monitor,
             std::unique_ptr<network::SimpleURLLoader> url_loader,
             std::unique_ptr<std::string> response) {
            if (monitor) {
              if (url_loader->NetError() != net::OK ||
                  !ParseReceiverSetupInfo(*response, &monitor->session_tags_,
                                          &monitor->receiver_name_))
                VLOG(2) << "Unable to fetch/parse receiver setup info.";
            }
          },
          weak_factory_.GetWeakPtr(), std::move(url_loader)),
      kMaxSetupResponseSizeBytes);
}

}  // namespace mirroring
