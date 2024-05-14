// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/database/download_db_conversions.h"

#include <utility>

#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/pickle.h"
#include "base/time/time.h"
#include "components/download/public/common/download_features.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

namespace download {
namespace {

// Converts base::Time to a timpstamp in milliseconds.
int64_t FromTimeToMilliseconds(base::Time time) {
  return time.ToDeltaSinceWindowsEpoch().InMilliseconds();
}

// Converts a time stamp in milliseconds to base::Time.
base::Time FromMillisecondsToTime(int64_t time_ms) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Milliseconds(time_ms));
}

}  // namespace

DownloadEntry DownloadDBConversions::DownloadEntryFromProto(
    const download_pb::DownloadEntry& proto) {
  DownloadEntry entry;
  entry.guid = proto.guid();
  entry.request_origin = proto.request_origin();
  entry.download_source = DownloadSourceFromProto(proto.download_source());
  entry.ukm_download_id = proto.ukm_download_id();
  entry.bytes_wasted = proto.bytes_wasted();
  entry.fetch_error_body = proto.fetch_error_body();
  for (const auto& header : proto.request_headers()) {
    entry.request_headers.emplace_back(HttpRequestHeaderFromProto(header));
  }
  return entry;
}

download_pb::DownloadEntry DownloadDBConversions::DownloadEntryToProto(
    const DownloadEntry& entry) {
  download_pb::DownloadEntry proto;
  proto.set_guid(entry.guid);
  proto.set_request_origin(entry.request_origin);
  proto.set_download_source(DownloadSourceToProto(entry.download_source));
  proto.set_ukm_download_id(entry.ukm_download_id);
  proto.set_bytes_wasted(entry.bytes_wasted);
  proto.set_fetch_error_body(entry.fetch_error_body);
  for (const auto& header : entry.request_headers) {
    auto* proto_header = proto.add_request_headers();
    *proto_header = HttpRequestHeaderToProto(header);
  }
  return proto;
}

// static
DownloadSource DownloadDBConversions::DownloadSourceFromProto(
    download_pb::DownloadSource download_source) {
  switch (download_source) {
    case download_pb::DownloadSource::UNKNOWN:
      return DownloadSource::UNKNOWN;
    case download_pb::DownloadSource::NAVIGATION:
      return DownloadSource::NAVIGATION;
    case download_pb::DownloadSource::DRAG_AND_DROP:
      return DownloadSource::DRAG_AND_DROP;
    case download_pb::DownloadSource::FROM_RENDERER:
      return DownloadSource::FROM_RENDERER;
    case download_pb::DownloadSource::EXTENSION_API:
      return DownloadSource::EXTENSION_API;
    case download_pb::DownloadSource::EXTENSION_INSTALLER:
      return DownloadSource::EXTENSION_INSTALLER;
    case download_pb::DownloadSource::INTERNAL_API:
      return DownloadSource::INTERNAL_API;
    case download_pb::DownloadSource::WEB_CONTENTS_API:
      return DownloadSource::WEB_CONTENTS_API;
    case download_pb::DownloadSource::OFFLINE_PAGE:
      return DownloadSource::OFFLINE_PAGE;
    case download_pb::DownloadSource::CONTEXT_MENU:
      return DownloadSource::CONTEXT_MENU;
    case download_pb::DownloadSource::RETRY:
      return DownloadSource::RETRY;
    case download_pb::DownloadSource::RETRY_FROM_BUBBLE:
      return DownloadSource::RETRY_FROM_BUBBLE;
    case download_pb::DownloadSource::TOOLBAR_MENU:
      return DownloadSource::TOOLBAR_MENU;
  }
  NOTREACHED_IN_MIGRATION();
  return DownloadSource::UNKNOWN;
}

// static
download_pb::DownloadSource DownloadDBConversions::DownloadSourceToProto(
    DownloadSource download_source) {
  switch (download_source) {
    case DownloadSource::UNKNOWN:
      return download_pb::DownloadSource::UNKNOWN;
    case DownloadSource::NAVIGATION:
      return download_pb::DownloadSource::NAVIGATION;
    case DownloadSource::DRAG_AND_DROP:
      return download_pb::DownloadSource::DRAG_AND_DROP;
    case DownloadSource::FROM_RENDERER:
      return download_pb::DownloadSource::FROM_RENDERER;
    case DownloadSource::EXTENSION_API:
      return download_pb::DownloadSource::EXTENSION_API;
    case DownloadSource::EXTENSION_INSTALLER:
      return download_pb::DownloadSource::EXTENSION_INSTALLER;
    case DownloadSource::INTERNAL_API:
      return download_pb::DownloadSource::INTERNAL_API;
    case DownloadSource::WEB_CONTENTS_API:
      return download_pb::DownloadSource::WEB_CONTENTS_API;
    case DownloadSource::OFFLINE_PAGE:
      return download_pb::DownloadSource::OFFLINE_PAGE;
    case DownloadSource::CONTEXT_MENU:
      return download_pb::DownloadSource::CONTEXT_MENU;
    case DownloadSource::RETRY:
      return download_pb::DownloadSource::RETRY;
    case DownloadSource::RETRY_FROM_BUBBLE:
      return download_pb::DownloadSource::RETRY_FROM_BUBBLE;
    case DownloadSource::TOOLBAR_MENU:
      return download_pb::DownloadSource::TOOLBAR_MENU;
  }
  NOTREACHED_IN_MIGRATION();
  return download_pb::DownloadSource::UNKNOWN;
}

std::vector<DownloadEntry> DownloadDBConversions::DownloadEntriesFromProto(
    const download_pb::DownloadEntries& proto) {
  std::vector<DownloadEntry> entries;
  for (int i = 0; i < proto.entries_size(); i++)
    entries.push_back(DownloadEntryFromProto(proto.entries(i)));
  return entries;
}

download_pb::DownloadEntries DownloadDBConversions::DownloadEntriesToProto(
    const std::vector<DownloadEntry>& entries) {
  download_pb::DownloadEntries proto;
  for (size_t i = 0; i < entries.size(); i++) {
    download_pb::DownloadEntry* proto_entry = proto.add_entries();
    *proto_entry = DownloadEntryToProto(entries[i]);
  }
  return proto;
}

// static
download_pb::HttpRequestHeader DownloadDBConversions::HttpRequestHeaderToProto(
    const std::pair<std::string, std::string>& header) {
  download_pb::HttpRequestHeader proto;
  if (header.first.empty())
    return proto;

  proto.set_key(header.first);
  proto.set_value(header.second);
  return proto;
}

// static
std::pair<std::string, std::string>
DownloadDBConversions::HttpRequestHeaderFromProto(
    const download_pb::HttpRequestHeader& proto) {
  if (proto.key().empty())
    return std::pair<std::string, std::string>();

  return std::make_pair(proto.key(), proto.value());
}

// static
download_pb::InProgressInfo DownloadDBConversions::InProgressInfoToProto(
    const InProgressInfo& in_progress_info) {
  download_pb::InProgressInfo proto;
  for (size_t i = 0; i < in_progress_info.url_chain.size(); ++i)
    proto.add_url_chain(in_progress_info.url_chain[i].spec());
  proto.set_referrer_url(in_progress_info.referrer_url.spec());
  proto.set_serialized_embedder_download_data(
      in_progress_info.serialized_embedder_download_data);
  proto.set_tab_url(in_progress_info.tab_url.spec());
  proto.set_tab_referrer_url(in_progress_info.tab_referrer_url.spec());
  proto.set_fetch_error_body(in_progress_info.fetch_error_body);
  for (const auto& header : in_progress_info.request_headers) {
    auto* proto_header = proto.add_request_headers();
    *proto_header = HttpRequestHeaderToProto(header);
  }
  proto.set_etag(in_progress_info.etag);
  proto.set_last_modified(in_progress_info.last_modified);
  proto.set_mime_type(in_progress_info.mime_type);
  proto.set_original_mime_type(in_progress_info.original_mime_type);
  proto.set_total_bytes(in_progress_info.total_bytes);
  base::Pickle current_path;
  in_progress_info.current_path.WriteToPickle(&current_path);
  proto.set_current_path(current_path.data(), current_path.size());
  base::Pickle target_path;
  in_progress_info.target_path.WriteToPickle(&target_path);
  proto.set_target_path(target_path.data(), target_path.size());
  proto.set_received_bytes(in_progress_info.received_bytes);
  proto.set_start_time(
      in_progress_info.start_time.is_null()
          ? -1
          : FromTimeToMilliseconds(in_progress_info.start_time));
  proto.set_end_time(in_progress_info.end_time.is_null()
                         ? -1
                         : FromTimeToMilliseconds(in_progress_info.end_time));
  for (size_t i = 0; i < in_progress_info.received_slices.size(); ++i) {
    download_pb::ReceivedSlice* slice = proto.add_received_slices();
    slice->set_received_bytes(
        in_progress_info.received_slices[i].received_bytes);
    slice->set_offset(in_progress_info.received_slices[i].offset);
    slice->set_finished(in_progress_info.received_slices[i].finished);
  }
  proto.set_hash(in_progress_info.hash);
  proto.set_transient(in_progress_info.transient);
  proto.set_state(in_progress_info.state);
  proto.set_danger_type(in_progress_info.danger_type);
  proto.set_interrupt_reason(in_progress_info.interrupt_reason);
  proto.set_paused(in_progress_info.paused);
  proto.set_metered(in_progress_info.metered);
  proto.set_bytes_wasted(in_progress_info.bytes_wasted);
  proto.set_auto_resume_count(in_progress_info.auto_resume_count);
  proto.set_credentials_mode(
      static_cast<int32_t>(in_progress_info.credentials_mode));
  proto.set_range_request_from(in_progress_info.range_request_from);
  proto.set_range_request_to(in_progress_info.range_request_to);
  return proto;
}

// static
InProgressInfo DownloadDBConversions::InProgressInfoFromProto(
    const download_pb::InProgressInfo& proto) {
  InProgressInfo info;
  for (const auto& url : proto.url_chain())
    info.url_chain.emplace_back(url);
  info.referrer_url = GURL(proto.referrer_url());
  info.serialized_embedder_download_data =
      proto.serialized_embedder_download_data();
  info.tab_url = GURL(proto.tab_url());
  info.tab_referrer_url = GURL(proto.tab_referrer_url());
  info.fetch_error_body = proto.fetch_error_body();
  for (const auto& header : proto.request_headers())
    info.request_headers.emplace_back(HttpRequestHeaderFromProto(header));
  info.etag = proto.etag();
  info.last_modified = proto.last_modified();
  info.mime_type = proto.mime_type();
  info.original_mime_type = proto.original_mime_type();
  info.total_bytes = proto.total_bytes();
  base::Pickle current_path_pickle =
      base::Pickle::WithUnownedBuffer(base::as_byte_span(proto.current_path()));
  base::PickleIterator current_path(current_path_pickle);
  info.current_path.ReadFromPickle(&current_path);
  base::Pickle target_path_pickle =
      base::Pickle::WithUnownedBuffer(base::as_byte_span(proto.target_path()));
  base::PickleIterator target_path(target_path_pickle);
  info.target_path.ReadFromPickle(&target_path);
  info.received_bytes = proto.received_bytes();
  info.start_time = proto.start_time() == -1
                        ? base::Time()
                        : FromMillisecondsToTime(proto.start_time());
  info.end_time = proto.end_time() == -1
                      ? base::Time()
                      : FromMillisecondsToTime(proto.end_time());

  for (int i = 0; i < proto.received_slices_size(); ++i) {
    info.received_slices.emplace_back(proto.received_slices(i).offset(),
                                      proto.received_slices(i).received_bytes(),
                                      proto.received_slices(i).finished());
  }
  info.hash = proto.hash();
  info.transient = proto.transient();
  info.state = static_cast<DownloadItem::DownloadState>(proto.state());
  info.danger_type = static_cast<DownloadDangerType>(proto.danger_type());
  info.interrupt_reason =
      static_cast<DownloadInterruptReason>(proto.interrupt_reason());
  info.paused = proto.paused();
  info.metered = proto.metered();
  info.bytes_wasted = proto.bytes_wasted();
  info.auto_resume_count = proto.auto_resume_count();
  if (proto.has_credentials_mode()) {
    info.credentials_mode = static_cast<::network::mojom::CredentialsMode>(
        proto.credentials_mode());
  }
  if (proto.has_range_request_from())
    info.range_request_from = proto.range_request_from();
  if (proto.has_range_request_to())
    info.range_request_to = proto.range_request_to();

  return info;
}

UkmInfo DownloadDBConversions::UkmInfoFromProto(
    const download_pb::UkmInfo& proto) {
  UkmInfo info;
  info.download_source = DownloadSourceFromProto(proto.download_source());
  info.ukm_download_id = proto.ukm_download_id();
  return info;
}

download_pb::UkmInfo DownloadDBConversions::UkmInfoToProto(
    const UkmInfo& info) {
  download_pb::UkmInfo proto;
  proto.set_download_source(DownloadSourceToProto(info.download_source));
  proto.set_ukm_download_id(info.ukm_download_id);
  return proto;
}

DownloadInfo DownloadDBConversions::DownloadInfoFromProto(
    const download_pb::DownloadInfo& proto) {
  DownloadInfo info;
  info.guid = proto.guid();
  info.id = proto.id();
  if (proto.has_ukm_info())
    info.ukm_info = UkmInfoFromProto(proto.ukm_info());
  if (proto.has_in_progress_info())
    info.in_progress_info = InProgressInfoFromProto(proto.in_progress_info());
  return info;
}

download_pb::DownloadInfo DownloadDBConversions::DownloadInfoToProto(
    const DownloadInfo& info) {
  download_pb::DownloadInfo proto;
  proto.set_guid(info.guid);
  proto.set_id(info.id);
  if (info.ukm_info.has_value()) {
    auto ukm_info = std::make_unique<download_pb::UkmInfo>(
        UkmInfoToProto(info.ukm_info.value()));
    proto.set_allocated_ukm_info(ukm_info.release());
  }
  if (info.in_progress_info.has_value()) {
    auto in_progress_info = std::make_unique<download_pb::InProgressInfo>(
        InProgressInfoToProto(info.in_progress_info.value()));
    proto.set_allocated_in_progress_info(in_progress_info.release());
  }
  return proto;
}

DownloadDBEntry DownloadDBConversions::DownloadDBEntryFromProto(
    const download_pb::DownloadDBEntry& proto) {
  DownloadDBEntry entry;
  if (proto.has_download_info())
    entry.download_info = DownloadInfoFromProto(proto.download_info());
  return entry;
}

download_pb::DownloadDBEntry DownloadDBConversions::DownloadDBEntryToProto(
    const DownloadDBEntry& info) {
  download_pb::DownloadDBEntry proto;
  if (info.download_info.has_value()) {
    auto download_info = std::make_unique<download_pb::DownloadInfo>(
        DownloadInfoToProto(info.download_info.value()));
    proto.set_allocated_download_info(download_info.release());
  }
  return proto;
}

DownloadDBEntry DownloadDBConversions::DownloadDBEntryFromDownloadEntry(
    const DownloadEntry& entry) {
  DownloadDBEntry db_entry;
  DownloadInfo download_info;
  download_info.guid = entry.guid;

  UkmInfo ukm_info(entry.download_source, entry.ukm_download_id);

  InProgressInfo in_progress_info;
  in_progress_info.fetch_error_body = entry.fetch_error_body;
  in_progress_info.request_headers = entry.request_headers;

  download_info.ukm_info = ukm_info;
  download_info.in_progress_info = in_progress_info;
  db_entry.download_info = download_info;
  return db_entry;
}

}  // namespace download
