// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_UTILITY_IMPORTER_METRICS_RECORDER_H_
#define COMPONENTS_USER_DATA_IMPORTER_UTILITY_IMPORTER_METRICS_RECORDER_H_

#include <cstdint>
#include <optional>
#include <string>

#include "base/time/time.h"
#include "components/user_data_importer/utility/bookmark_parser.h"

/**
 * This file contains a class for logging metrics which are broadly applicable
 * to importing user data. Some data types (i.e., instances of DataTypeMetrics)
 * may not apply to all importers, and importers may include additional logging
 * beyond these common metrics in their implementations.
 */

namespace user_data_importer {

// Specific error states for Bookmarks import.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(BookmarksImportError)
enum class BookmarksImportError {
  kFailedToRead = 0,
  kTooBig = 1,
  kParsingFailed = 2,
  kTimeout = 3,
  kOther = 4,
  kMaxValue = kOther,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/user_data_importer/enums.xml:UserDataImportBookmarksReadingListError)

// Helper function to convert from the parser's error enum to the metrics enum.
BookmarksImportError ConvertBookmarkError(
    BookmarkParser::BookmarkParsingError error);

// Specific error states for Passwords import.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PasswordsImportError)
enum class PasswordsImportError {
  kFailedToRead = 0,
  kTooBig = 1,
  kParsingFailed = 2,
  kTimeout = 3,
  kOther = 4,
  kMaxValue = kOther,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/user_data_importer/enums.xml:UserDataImportPasswordsError)

// Represents metrics for a single data type. Encapsulates the data common
// to all supported data types.
class DataTypeMetrics {
 public:
  // Indicates the type of data being imported. Corresponds to the
  // ImportDataType variants in histograms.xml, with the exception of
  // NotSupported, which is not supported.
  // LINT.IfChange(ImportDataType)
  enum class DataType {
    kBookmarks = 0,
    kHistory = 1,
    kPasswords = 2,
    kPaymentCards = 3,
    kReadingList = 4
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/user_data_importer/histograms.xml:ImportDataType)

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(ImportOutcome)
  enum class ImportOutcome {
    kNotPresent = 0,  // No file for this data type was provided.
    kFailure = 1,     // A file was processed but no data could be imported.
    kSuccess = 2,     // At least one data entry was successfully imported.
    kMaxValue = kSuccess,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/user_data_importer/enums.xml:UserDataImportOutcome)

  DataTypeMetrics(const std::string& source_name, const DataType data_type);
  ~DataTypeMetrics();

  // Methods to log metrics.
  void LogOutcome(ImportOutcome outcome);
  void LogFileSizeBytes(int64_t size_bytes);

  // Timestamp recording methods
  void OnPreparationStarted();
  void OnPreparationFinished(size_t prepared_count);
  void OnImportStarted();
  void OnImportFinished(size_t completed_count);

 private:
  std::string GetMetricName(std::string_view suffix);

  const std::string source_name_;
  const std::string data_type_name_;

  std::optional<base::TimeTicks> parse_start_time_;
  std::optional<base::TimeTicks> import_start_time_;

  size_t prepared_count_ = 0;
};

// Logs metrics accumulated in the process of importing user data.
class ImporterMetricsRecorder {
 public:
  // Indicates the source of the data being imported.
  // LINT.IfChange(ImportSource)
  enum class Source {
    kOsMigration = 0,
    kSafari = 1,
    kStablePortabilityData = 2,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/user_data_importer/histograms.xml:ImportSource)

  explicit ImporterMetricsRecorder(const Source source);
  ~ImporterMetricsRecorder();

  // Call to record the start of the import process.
  void OnFlowStarted();

  // Call to record the end of the import process.
  void OnFlowFinished();

  // Accessors for logging metrics common to each data type.
  DataTypeMetrics& bookmark_metrics() { return bookmark_metrics_; }
  DataTypeMetrics& history_metrics() { return history_metrics_; }
  DataTypeMetrics& password_metrics() { return password_metrics_; }
  DataTypeMetrics& payment_card_metrics() { return payment_card_metrics_; }
  DataTypeMetrics& reading_list_metrics() { return reading_list_metrics_; }

  // Log specific failure cases, as documented in the respective enums above.
  void LogBookmarksError(BookmarksImportError error);
  void LogReadingListError(BookmarksImportError error);
  void LogPasswordsError(PasswordsImportError error);

 private:
  std::string GetMetricName(std::string_view suffix);

  const std::string source_name_;

  base::TimeTicks flow_start_time_;

  DataTypeMetrics bookmark_metrics_;
  DataTypeMetrics history_metrics_;
  DataTypeMetrics password_metrics_;
  DataTypeMetrics payment_card_metrics_;
  DataTypeMetrics reading_list_metrics_;
};

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_UTILITY_IMPORTER_METRICS_RECORDER_H_
