// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/utility/importer_metrics_recorder.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"

namespace user_data_importer {

BookmarksImportError ConvertBookmarkError(
    BookmarkParser::BookmarkParsingError error) {
  switch (error) {
    case BookmarkParser::BookmarkParsingError::kFailedToReadFile:
      return BookmarksImportError::kFailedToRead;
    case BookmarkParser::BookmarkParsingError::kTooBig:
      return BookmarksImportError::kTooBig;
    case BookmarkParser::BookmarkParsingError::kParsingFailed:
      return BookmarksImportError::kParsingFailed;
    case BookmarkParser::BookmarkParsingError::kTimedOut:
      return BookmarksImportError::kTimeout;
    case BookmarkParser::BookmarkParsingError::kOther:
      return BookmarksImportError::kOther;
    default:
      NOTREACHED(base::NotFatalUntil::M145)
          << "Unknown error: " << static_cast<int>(error);
      return BookmarksImportError::kOther;
  }
}

namespace {

// These values must match those declared in the corresponding histograms.xml
// file, as they are used to construct histogram names.
std::string GetSourceName(ImporterMetricsRecorder::Source source) {
  switch (source) {
    case ImporterMetricsRecorder::Source::kOsMigration:
      return "OSMigration";
    case ImporterMetricsRecorder::Source::kSafari:
      return "Safari";
    case ImporterMetricsRecorder::Source::kStablePortabilityData:
      return "StablePortabilityData";
  }
}

// These values must match those declared in the corresponding histograms.xml
// file, as they are used to construct histogram names.
std::string GetDataTypeName(DataTypeMetrics::DataType data_type) {
  switch (data_type) {
    case DataTypeMetrics::DataType::kBookmarks:
      return "Bookmarks";
    case DataTypeMetrics::DataType::kHistory:
      return "History";
    case DataTypeMetrics::DataType::kPasswords:
      return "Passwords";
    case DataTypeMetrics::DataType::kPaymentCards:
      return "PaymentCards";
    case DataTypeMetrics::DataType::kReadingList:
      return "ReadingList";
  }
}

}  // namespace

DataTypeMetrics::DataTypeMetrics(const std::string& source_name,
                                 const DataTypeMetrics::DataType data_type)
    : source_name_(source_name), data_type_name_(GetDataTypeName(data_type)) {}

DataTypeMetrics::~DataTypeMetrics() = default;

void DataTypeMetrics::LogOutcome(ImportOutcome outcome) {
  base::UmaHistogramEnumeration(GetMetricName("Outcome"), outcome);
}

void DataTypeMetrics::LogFileSizeBytes(int64_t size_bytes) {
  if (size_bytes < 0) {
    return;
  }

  int file_size_kb = static_cast<int>(size_bytes / 1024);
  base::UmaHistogramMemoryKB(GetMetricName("FileSize"), file_size_kb);
}

void DataTypeMetrics::OnPreparationStarted() {
  parse_start_time_ = base::TimeTicks::Now();
}

void DataTypeMetrics::OnPreparationFinished(size_t prepared_count) {
  prepared_count_ = prepared_count;

  if (parse_start_time_) {
    base::TimeDelta duration = base::TimeTicks::Now() - *parse_start_time_;
    base::UmaHistogramCustomTimes(GetMetricName("PrepareDuration"), duration,
                                  base::Milliseconds(1), base::Minutes(10),
                                  /*buckets=*/50);
    parse_start_time_.reset();
  }
  base::UmaHistogramCounts10000(GetMetricName("PreparedCount"),
                                prepared_count_);
}

void DataTypeMetrics::OnImportStarted() {
  import_start_time_ = base::TimeTicks::Now();
}

void DataTypeMetrics::OnImportFinished(size_t completed_count) {
  if (import_start_time_) {
    base::TimeDelta duration = base::TimeTicks::Now() - *import_start_time_;
    base::UmaHistogramCustomTimes(GetMetricName("ImportDuration"), duration,
                                  base::Milliseconds(1), base::Minutes(10),
                                  /*buckets=*/50);
    import_start_time_.reset();
  }

  base::UmaHistogramCounts10000(GetMetricName("ImportedCount"),
                                completed_count);

  if (prepared_count_ > 0) {
    int success_percentage = static_cast<int>(
        (static_cast<double>(completed_count) / prepared_count_) * 100.0);
    base::UmaHistogramPercentage(GetMetricName("SuccessRate"),
                                 success_percentage);
  }
}

std::string DataTypeMetrics::GetMetricName(std::string_view suffix) {
  return base::JoinString(
      {"UserDataImporter", source_name_, data_type_name_, suffix},
      /*separator=*/".");
}

ImporterMetricsRecorder::ImporterMetricsRecorder(
    ImporterMetricsRecorder::Source source)
    : source_name_(GetSourceName(source)),
      bookmark_metrics_(source_name_, DataTypeMetrics::DataType::kBookmarks),
      history_metrics_(source_name_, DataTypeMetrics::DataType::kHistory),
      password_metrics_(source_name_, DataTypeMetrics::DataType::kPasswords),
      payment_card_metrics_(source_name_,
                            DataTypeMetrics::DataType::kPaymentCards),
      reading_list_metrics_(source_name_,
                            DataTypeMetrics::DataType::kReadingList) {}

ImporterMetricsRecorder::~ImporterMetricsRecorder() = default;

void ImporterMetricsRecorder::OnFlowStarted() {
  flow_start_time_ = base::TimeTicks::Now();
}

void ImporterMetricsRecorder::OnFlowFinished() {
  base::TimeTicks flow_end_time = base::TimeTicks::Now();

  // Log the total flow duration.
  base::TimeDelta duration = flow_end_time - flow_start_time_;
  base::UmaHistogramCustomTimes(GetMetricName("FlowDuration"), duration,
                                base::Milliseconds(1), base::Minutes(10),
                                /*buckets=*/50);
}

void ImporterMetricsRecorder::LogBookmarksError(BookmarksImportError error) {
  base::UmaHistogramEnumeration(GetMetricName("Bookmarks.Error"), error);
}

void ImporterMetricsRecorder::LogReadingListError(BookmarksImportError error) {
  base::UmaHistogramEnumeration(GetMetricName("ReadingList.Error"), error);
}

void ImporterMetricsRecorder::LogPasswordsError(PasswordsImportError error) {
  base::UmaHistogramEnumeration(GetMetricName("Passwords.Error"), error);
}

std::string ImporterMetricsRecorder::GetMetricName(std::string_view suffix) {
  return base::JoinString({"UserDataImporter", source_name_, suffix},
                          /*separator=*/".");
}

}  // namespace user_data_importer
