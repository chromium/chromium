// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_DB_CONVERSIONS_H_
#define COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_DB_CONVERSIONS_H_

#include "components/download/database/download_db_entry.h"
#include "components/download/database/download_info.h"
#include "components/download/database/download_namespace.h"
#include "components/download/database/in_progress/download_entry.h"
#include "components/download/database/in_progress/in_progress_info.h"
#include "components/download/database/in_progress/ukm_info.h"
#include "components/download/database/proto/download_entry.pb.h"
#include "components/download/database/proto/download_source.pb.h"

namespace download {

class DownloadDBConversions {
 public:
  static DownloadEntry DownloadEntryFromProto(
      const download_pb::DownloadEntry& proto);

  static download_pb::DownloadEntry DownloadEntryToProto(
      const DownloadEntry& entry);

  static DownloadSource DownloadSourceFromProto(
      download_pb::DownloadSource download_source);

  static download_pb::DownloadSource DownloadSourceToProto(
      DownloadSource download_source);

  static std::vector<DownloadEntry> DownloadEntriesFromProto(
      const download_pb::DownloadEntries& proto);

  static download_pb::DownloadEntries DownloadEntriesToProto(
      const std::vector<DownloadEntry>& entries);

  static download_pb::HttpRequestHeader HttpRequestHeaderToProto(
      const std::pair<std::string, std::string>& header);

  static std::pair<std::string, std::string> HttpRequestHeaderFromProto(
      const download_pb::HttpRequestHeader& proto);

  static download_pb::InProgressInfo InProgressInfoToProto(
      const InProgressInfo& in_progress_info);

  static InProgressInfo InProgressInfoFromProto(
      const download_pb::InProgressInfo& proto);

  static download_pb::UkmInfo UkmInfoToProto(const UkmInfo& ukm_info);

  static UkmInfo UkmInfoFromProto(const download_pb::UkmInfo& proto);

  static download_pb::DownloadInfo DownloadInfoToProto(
      const DownloadInfo& download_info);

  static DownloadInfo DownloadInfoFromProto(
      const download_pb::DownloadInfo& proto);

  static download_pb::DownloadDBEntry DownloadDBEntryToProto(
      const DownloadDBEntry& entry);

  static DownloadDBEntry DownloadDBEntryFromProto(
      const download_pb::DownloadDBEntry& proto);

  static DownloadDBEntry DownloadDBEntryFromDownloadEntry(
      const DownloadEntry& entry);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_DB_CONVERSIONS_H_
