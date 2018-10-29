// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/registry_logger.h"

#include <stdint.h>
#include <set>

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/constants/version.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

const wchar_t kLogFilePath1[] = L"AFilePath";
const wchar_t kLogFilePath2[] = L"AnotherOne";
const wchar_t kLogFilePath3[] = L"ThirdsTheCharm";
const char kCorruptFilePath[] = "x";
const wchar_t kWideCorruptFilePath[] = L"x";
const wchar_t kRawFilePath[] = L"AFilePath\0\0";
const char kTestSuffix[] = "TestSuffix";
const char kInvalidUTF8[] = "\xed\xa0\x80\xed\xbf\xbf";

class TestRegistryLogger : public RegistryLogger {
 public:
  explicit TestRegistryLogger(RegistryLogger::Mode mode)
      : RegistryLogger(mode) {}
  TestRegistryLogger(RegistryLogger::Mode mode, std::string suffix)
      : RegistryLogger(mode, suffix) {}
  using RegistryLogger::GetLoggingKeyPath;
  using RegistryLogger::GetScanTimesKeyPath;
  using RegistryLogger::GetKeySuffix;
  using RegistryLogger::ReadValues;
  using RegistryLogger::kMaxUploadResultLength;
  using RegistryLogger::kPendingLogFilesValue;
};

bool ReadFoundPUPs(std::set<UwSId>* stored_pups) {
  DCHECK(stored_pups);

  const RegistryLogger::Mode mode = RegistryLogger::Mode::REMOVER;
  TestRegistryLogger logger(mode);
  base::win::RegKey logging_key;
  logging_key.Open(HKEY_CURRENT_USER, logger.GetLoggingKeyPath(mode).c_str(),
                   KEY_QUERY_VALUE);
  if (!logging_key.Valid())
    return false;

  std::vector<base::string16> existing_pups;
  bool success = true;
  if (logger.ReadValues(logging_key, kFoundUwsValueName, &existing_pups,
                        nullptr)) {
    for (base::string16 pup_id_string : existing_pups) {
      UwSId pup_id = 0;
      if (base::StringToUint(pup_id_string, &pup_id))
        stored_pups->insert(pup_id);
      else
        success = false;
    }
  }
  return success;
}

// Open |reg_key| using GetLoggingKeyPath(). Returns true if the key is
// created properly, false otherwise.
bool OpenLoggingRegKey(base::win::RegKey* reg_key,
                       const TestRegistryLogger& logger,
                       const REGSAM access,
                       const RegistryLogger::Mode mode) {
  LONG result = reg_key->Open(HKEY_CURRENT_USER,
                              logger.GetLoggingKeyPath(mode).c_str(), access);
  return reg_key->Valid() && (result == ERROR_SUCCESS);
}

// Open |reg_key| using GetScanTimesKeyPath(). Returns true if the key is
// created properly, false otherwise.
bool OpenScanTimeRegKey(base::win::RegKey* reg_key,
                        const TestRegistryLogger& logger,
                        const REGSAM access,
                        const RegistryLogger::Mode mode) {
  LONG result = reg_key->Open(HKEY_CURRENT_USER,
                              logger.GetScanTimesKeyPath(mode).c_str(), access);
  return reg_key->Valid() && (result == ERROR_SUCCESS);
}

class RegistryLoggerTest : public testing::Test {
 public:
  void SetUp() override {
    registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER);
  }

 protected:
  registry_util::RegistryOverrideManager registry_override_manager_;
};

TEST_F(RegistryLoggerTest, AppendLogUploadResultBasic) {
  const RegistryLogger::Mode mode = RegistryLogger::Mode::REMOVER;
  TestRegistryLogger logger(mode);
  logger.AppendLogUploadResult(true);
  logger.AppendLogUploadResult(false);
  logger.AppendLogUploadResult(false);

  base::win::RegKey logging_key;
  ASSERT_TRUE(OpenLoggingRegKey(&logging_key, logger, KEY_QUERY_VALUE, mode));

  base::string16 upload_results;
  LONG result = logging_key.ReadValue(kUploadResultsValueName, &upload_results);
  EXPECT_EQ(ERROR_SUCCESS, result);
  EXPECT_STREQ(L"1;0;0;", upload_results.c_str());
}

TEST_F(RegistryLoggerTest, WriteReporterLogsUploadResult) {
  const RegistryLogger::Mode mode = RegistryLogger::Mode::REPORTER;
  TestRegistryLogger logger(mode);
  logger.WriteReporterLogsUploadResult(
      SafeBrowsingReporter::Result::UPLOAD_INTERNAL_ERROR);

  base::win::RegKey logging_key;
  ASSERT_TRUE(OpenLoggingRegKey(&logging_key, logger, KEY_QUERY_VALUE, mode));
  DWORD upload_result;
  EXPECT_EQ(ERROR_SUCCESS, logging_key.ReadValueDW(kLogsUploadResultValueName,
                                                   &upload_result));
  EXPECT_EQ(
      static_cast<DWORD>(SafeBrowsingReporter::Result::UPLOAD_INTERNAL_ERROR),
      upload_result);
}

TEST_F(RegistryLoggerTest, WriteAndClearScanTimes) {
  const RegistryLogger::Mode mode = RegistryLogger::Mode::REPORTER;
  TestRegistryLogger logger(mode);
  base::TimeDelta scan_time = base::TimeDelta::FromSeconds(3);
  logger.WriteScanTime(42, scan_time);

  base::win::RegKey scan_times_key;
  ASSERT_TRUE(
      OpenScanTimeRegKey(&scan_times_key, logger, KEY_QUERY_VALUE, mode));

  int64_t us_scan_time = 0;
  LONG result = scan_times_key.ReadInt64(L"42", &us_scan_time);
  EXPECT_EQ(ERROR_SUCCESS, result);
  base::TimeDelta read_scan_time =
      base::TimeDelta::FromMicroseconds(us_scan_time);

  EXPECT_EQ(read_scan_time, scan_time);

  logger.ClearScanTimes();
  ASSERT_TRUE(
      OpenScanTimeRegKey(&scan_times_key, logger, KEY_QUERY_VALUE, mode));
  EXPECT_EQ(0U, scan_times_key.GetValueCount());
}

TEST_F(RegistryLoggerTest, AppendLogUploadResultMaxLength) {
  const RegistryLogger::Mode mode = RegistryLogger::Mode::REMOVER;
  TestRegistryLogger logger(RegistryLogger::Mode::REMOVER);

  static const size_t kMaxNumUploadResults =
      TestRegistryLogger::kMaxUploadResultLength / 2;

  for (size_t i = 0; i < kMaxNumUploadResults; ++i)
    logger.AppendLogUploadResult(true);

  base::win::RegKey logging_key;
  ASSERT_TRUE(OpenLoggingRegKey(&logging_key, logger, KEY_QUERY_VALUE, mode));

  base::string16 upload_results;
  LONG result = logging_key.ReadValue(kUploadResultsValueName, &upload_results);
  EXPECT_EQ(ERROR_SUCCESS, result);

  base::string16 expected_string;
  for (size_t i = 0; i < kMaxNumUploadResults; ++i)
    expected_string += L"1;";

  EXPECT_EQ(expected_string, upload_results);

  logger.AppendLogUploadResult(false);
  result = logging_key.ReadValue(kUploadResultsValueName, &upload_results);
  EXPECT_EQ(ERROR_SUCCESS, result);

  expected_string[expected_string.length() - 2] = '0';

  EXPECT_EQ(expected_string, upload_results);
}

TEST_F(RegistryLoggerTest, LogFilePath) {
  TestRegistryLogger logger(RegistryLogger::Mode::REMOVER);

  base::FilePath log_file;
  base::FilePath log_file_path1(kLogFilePath1);
  EXPECT_TRUE(logger.AppendLogFilePath(log_file_path1));
  logger.GetNextLogFilePath(&log_file);
  EXPECT_EQ(log_file_path1, log_file);

  base::FilePath log_file_path2(kLogFilePath2);
  EXPECT_TRUE(logger.AppendLogFilePath(log_file_path2));
  logger.GetNextLogFilePath(&log_file);
  EXPECT_EQ(log_file_path1, log_file);

  // If we remove the second file, there is still the first file.
  EXPECT_TRUE(logger.RemoveLogFilePath(log_file_path2));
  logger.GetNextLogFilePath(&log_file);
  EXPECT_EQ(log_file_path1, log_file);

  // Also test removing the first file in the list.
  EXPECT_TRUE(logger.AppendLogFilePath(log_file_path2));
  EXPECT_TRUE(logger.RemoveLogFilePath(log_file_path1));
  logger.GetNextLogFilePath(&log_file);
  EXPECT_EQ(log_file_path2, log_file);

  // And now middle file removal.
  base::FilePath log_file_path3(kLogFilePath3);
  EXPECT_TRUE(logger.AppendLogFilePath(log_file_path1));
  EXPECT_TRUE(logger.AppendLogFilePath(log_file_path3));
  EXPECT_TRUE(logger.RemoveLogFilePath(log_file_path1));
  logger.GetNextLogFilePath(&log_file);
  EXPECT_EQ(log_file_path2, log_file);

  EXPECT_TRUE(logger.RemoveLogFilePath(log_file_path2));
  logger.GetNextLogFilePath(&log_file);
  EXPECT_EQ(log_file_path3, log_file);
  EXPECT_FALSE(logger.RemoveLogFilePath(log_file_path3));
  logger.GetNextLogFilePath(&log_file);
  EXPECT_TRUE(log_file.empty());
}

TEST_F(RegistryLoggerTest, LogFilePathFailures) {
  const RegistryLogger::Mode mode = RegistryLogger::Mode::REMOVER;
  TestRegistryLogger logger(mode);

  // Make sure we get an empty file path when there's no value.
  base::FilePath log_file;
  logger.GetNextLogFilePath(&log_file);
  EXPECT_TRUE(log_file.empty());

  // Create an empty value, and make sure we also get an empty file path.
  base::win::RegKey logging_key;
  ASSERT_TRUE(OpenLoggingRegKey(&logging_key, logger, KEY_SET_VALUE, mode));

  logging_key.WriteValue(TestRegistryLogger::kPendingLogFilesValue, L"");
  logger.GetNextLogFilePath(&log_file);
  EXPECT_TRUE(log_file.empty());
  // And the value should have been wiped.
  EXPECT_FALSE(logging_key.HasValue(TestRegistryLogger::kPendingLogFilesValue));

  // Now try with an invalid value type.
  logging_key.WriteValue(TestRegistryLogger::kPendingLogFilesValue, 42);
  logger.GetNextLogFilePath(&log_file);
  EXPECT_TRUE(log_file.empty());
  // And again, the value should have been wiped.
  EXPECT_FALSE(logging_key.HasValue(TestRegistryLogger::kPendingLogFilesValue));
}

TEST_F(RegistryLoggerTest, LogFilePathRegistryFailure) {
  TestRegistryLogger logger(RegistryLogger::Mode::NOOP_FOR_TESTING);

  base::FilePath log_file_path1(kLogFilePath1);
  EXPECT_FALSE(logger.AppendLogFilePath(log_file_path1));
  EXPECT_FALSE(logger.RemoveLogFilePath(log_file_path1));

  base::FilePath log_file;
  logger.GetNextLogFilePath(&log_file);
  EXPECT_TRUE(log_file.empty());
}

TEST_F(RegistryLoggerTest, LogFilePathWithCorruptKey) {
  const RegistryLogger::Mode mode = RegistryLogger::Mode::REMOVER;
  TestRegistryLogger logger(mode);

  base::win::RegKey logging_key;
  ASSERT_TRUE(OpenLoggingRegKey(&logging_key, logger, KEY_SET_VALUE, mode));

  // Write a corrupt registry key (one byte, no ending null character).
  logging_key.WriteValue(TestRegistryLogger::kPendingLogFilesValue,
                         kCorruptFilePath, 1, REG_MULTI_SZ);

  base::FilePath log_file;
  logger.GetNextLogFilePath(&log_file);
  EXPECT_EQ(kWideCorruptFilePath, log_file.value());
}

TEST_F(RegistryLoggerTest, LogFilePathRawContent) {
  const RegistryLogger::Mode mode = RegistryLogger::Mode::REMOVER;
  TestRegistryLogger logger(mode);

  base::win::RegKey logging_key;
  ASSERT_TRUE(OpenLoggingRegKey(&logging_key, logger, KEY_SET_VALUE, mode));

  // Try every possibility of ending trailing null characters.
  size_t length = wcslen(kRawFilePath) * sizeof(wchar_t);
  for (; length < sizeof(kRawFilePath); ++length) {
    logging_key.WriteValue(TestRegistryLogger::kPendingLogFilesValue,
                           kRawFilePath, length, REG_MULTI_SZ);

    base::FilePath log_file;
    logger.GetNextLogFilePath(&log_file);
    EXPECT_EQ(kRawFilePath, log_file.value());
  }
}

TEST_F(RegistryLoggerTest, RecordFoundPUPs) {
  TestRegistryLogger logger(RegistryLogger::Mode::REMOVER);

  std::set<UwSId> stored_pups;
  ReadFoundPUPs(&stored_pups);
  EXPECT_TRUE(stored_pups.empty());

  std::vector<UwSId> pup_list{1, 2, 3, 4, 5};
  EXPECT_TRUE(logger.RecordFoundPUPs(pup_list));
  EXPECT_TRUE(ReadFoundPUPs(&stored_pups));

  std::set<UwSId> expected_pups{1, 2, 3, 4, 5};
  for (UwSId pup_id : stored_pups)
    EXPECT_EQ(1UL, expected_pups.count(pup_id));
  EXPECT_EQ(expected_pups.size(), stored_pups.size());

  std::vector<UwSId> pup_list2{6};
  EXPECT_TRUE(logger.RecordFoundPUPs(pup_list2));
  EXPECT_TRUE(ReadFoundPUPs(&stored_pups));

  std::set<UwSId> expected_pups2{1, 2, 3, 4, 5, 6};
  for (UwSId pup_id : stored_pups)
    EXPECT_EQ(1UL, expected_pups2.count(pup_id));
  EXPECT_EQ(expected_pups2.size(), stored_pups.size());

  std::vector<UwSId> pup_list3{5, 6, 7, 8};
  EXPECT_TRUE(logger.RecordFoundPUPs(pup_list3));
  EXPECT_TRUE(ReadFoundPUPs(&stored_pups));

  std::set<UwSId> expected_pups3{1, 2, 3, 4, 5, 6, 7, 8};
  for (UwSId pup_id : stored_pups)
    EXPECT_EQ(1UL, expected_pups3.count(pup_id));
  EXPECT_EQ(expected_pups3.size(), stored_pups.size());
}

TEST_F(RegistryLoggerTest, RecordFoundPUPsDuplicates) {
  TestRegistryLogger logger(RegistryLogger::Mode::REMOVER);

  std::set<UwSId> stored_pups;
  ReadFoundPUPs(&stored_pups);
  EXPECT_TRUE(stored_pups.empty());

  std::vector<UwSId> pup_list{1, 1, 2, 2, 3};
  EXPECT_TRUE(logger.RecordFoundPUPs(pup_list));
  EXPECT_TRUE(ReadFoundPUPs(&stored_pups));

  std::set<UwSId> expected_pups{1, 2, 3};
  for (UwSId pup_id : stored_pups)
    EXPECT_EQ(1UL, expected_pups.count(pup_id));
}

TEST_F(RegistryLoggerTest, LoggingKeyPathContainsCompanyName) {
  const RegistryLogger::Mode mode = RegistryLogger::Mode::REPORTER;
  TestRegistryLogger logger(mode);
  // This checks directly for the company name Google, instead of using
  // COMPANY_SHORTNAME_STRING, because it's used for communication with Chrome
  // so it needs to use Chrome's company name.
  const base::string16 expected_name =
      L"Software\\Google\\Software Removal Tool";
  EXPECT_EQ(expected_name, logger.GetLoggingKeyPath(mode));
}

TEST_F(RegistryLoggerTest, UseSuffixRegistryKey) {
  const RegistryLogger::Mode mode = RegistryLogger::Mode::REPORTER;
  TestRegistryLogger logger(mode, kTestSuffix);
  TestRegistryLogger no_suffix_logger(mode);

  base::string16 key_name = no_suffix_logger.GetLoggingKeyPath(mode);
  EXPECT_EQ(base::string16::npos,
            key_name.find(base::UTF8ToUTF16(kTestSuffix).c_str()));

  // This checks directly for the company name Google, instead of using
  // COMPANY_SHORTNAME_STRING, because it's used for communication with Chrome
  // so it needs to use Chrome's company name.
  key_name = logger.GetLoggingKeyPath(mode);
  const base::string16 expected_name =
      base::StrCat({L"Software\\Google\\Software Removal Tool\\",
                    base::UTF8ToUTF16(kTestSuffix)});
  EXPECT_EQ(expected_name, key_name);

  base::win::RegKey no_suffix_key;
  EXPECT_TRUE(
      OpenLoggingRegKey(&no_suffix_key, no_suffix_logger, KEY_SET_VALUE, mode));

  base::win::RegKey logger_key;
  EXPECT_TRUE(OpenLoggingRegKey(&logger_key, logger, KEY_SET_VALUE, mode));
}

TEST_F(RegistryLoggerTest, InvalidRegistrySuffixes) {
  const RegistryLogger::Mode mode = RegistryLogger::Mode::REPORTER;
  // Invalid characters in suffix.
  TestRegistryLogger logger_invalid_utf8(mode, kInvalidUTF8);

  base::win::RegKey invalid_utf8_key;
  EXPECT_FALSE(OpenLoggingRegKey(&invalid_utf8_key, logger_invalid_utf8,
                                 KEY_SET_VALUE, mode));

  // Suffix too long.
  std::string suffix_too_long(0x4000, 'A');
  TestRegistryLogger logger_too_long(RegistryLogger::Mode::REPORTER,
                                     suffix_too_long);
  base::win::RegKey too_long_key;
  EXPECT_FALSE(
      OpenLoggingRegKey(&too_long_key, logger_too_long, KEY_SET_VALUE, mode));
}

TEST_F(RegistryLoggerTest, WriteExperimentalEngineResultCode) {
  const RegistryLogger::Mode mode = RegistryLogger::Mode::REPORTER;
  TestRegistryLogger logger(mode);
  logger.WriteExperimentalEngineResultCode(42);

  base::win::RegKey logging_key;
  ASSERT_TRUE(OpenLoggingRegKey(&logging_key, logger, KEY_QUERY_VALUE, mode));
  DWORD engine_error_code;
  EXPECT_EQ(ERROR_SUCCESS, logging_key.ReadValueDW(kEngineErrorCodeValueName,
                                                   &engine_error_code));
  EXPECT_EQ(static_cast<DWORD>(42), engine_error_code);
}

TEST_F(RegistryLoggerTest, RecordAndResetCompletedCleanup) {
  const RegistryLogger::Mode mode = RegistryLogger::Mode::REMOVER;
  TestRegistryLogger logger(mode);

  logger.RecordCompletedCleanup();
  {
    base::win::RegKey logging_key;
    ASSERT_TRUE(OpenLoggingRegKey(&logging_key, logger, KEY_QUERY_VALUE, mode));
    DWORD recorded_value;
    EXPECT_EQ(ERROR_SUCCESS, logging_key.ReadValueDW(kCleanupCompletedValueName,
                                                     &recorded_value));
    EXPECT_EQ(static_cast<DWORD>(1), recorded_value);
  }

  logger.ResetCompletedCleanup();
  {
    base::win::RegKey logging_key;
    ASSERT_TRUE(OpenLoggingRegKey(&logging_key, logger, KEY_QUERY_VALUE, mode));
    EXPECT_FALSE(logging_key.HasValue(kCleanupCompletedValueName));
  }
}

}  // namespace chrome_cleaner
