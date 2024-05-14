// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/biod/fake_biod_client.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

// Path of an enroll session. There should only be one enroll session at a
// given time.
const char kEnrollSessionObjectPath[] = "/EnrollSession";

// Header of the path of an record. A unique number will be appended when an
// record is created.
const char kRecordObjectPathPrefix[] = "/Record/";

// Path of an auth session. There should only be one auth sesion at a given
// time.
const char kAuthSessionObjectPath[] = "/AuthSession";

FakeBiodClient* g_instance = nullptr;

FakeBiodClient::FakeRecord ParseFakeRecordDict(
    const base::Value::Dict& fake_record_dict) {
  FakeBiodClient::FakeRecord res;
  for (const auto [key, value] : fake_record_dict) {
    if (key == "fingerprints") {
      for (const auto& fp_entry : value.GetList()) {
        res.fake_fingerprint.push_back(fp_entry.GetString());
      }
    } else if (key == "user_id") {
      res.user_id = value.GetString();
    } else if (key == "label") {
      res.label = value.GetString();
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }
  return res;
}

base::Value::Dict FakeRecordsToValue(const FakeBiodClient::RecordMap& records) {
  base::Value::Dict res;
  for (const auto& entry : records) {
    const std::string& entry_key = entry.first.value();
    const FakeBiodClient::FakeRecord& entry_fake_record = entry.second;
    base::Value::List fake_images;
    for (const std::string& fake_image : entry_fake_record.fake_fingerprint) {
      fake_images.Append(fake_image);
    }
    base::Value::Dict cur_record;
    cur_record.Set("fingerprints", std::move(fake_images));
    cur_record.Set("user_id", entry_fake_record.user_id);
    cur_record.Set("label", entry_fake_record.label);
    res.Set(entry_key, std::move(cur_record));
  }
  return res;
}

FakeBiodClient::RecordMap ValueToFakeRecords(const base::Value& records_val) {
  FakeBiodClient::RecordMap records;
  const base::Value::Dict& fake_biod_db_dict = records_val.GetDict();
  for (const auto fake_biod_db_entry : fake_biod_db_dict) {
    const base::Value::Dict& fake_record_dict =
        fake_biod_db_entry.second.GetDict();
    records.try_emplace(dbus::ObjectPath(fake_biod_db_entry.first),
                        ParseFakeRecordDict(fake_record_dict));
  }
  return records;
}

int GetNextRecordId(const FakeBiodClient::RecordMap& records) {
  int next_record_unique_id = 1;
  for (const auto& [key, _] : records) {
    std::vector<std::string_view> splitted_str = base::SplitStringPiece(
        key.value(), "/", base::WhitespaceHandling::TRIM_WHITESPACE,
        base::SplitResult::SPLIT_WANT_NONEMPTY);
    CHECK_EQ(splitted_str.size(), static_cast<size_t>(2));
    int record_id = 0;
    CHECK(base::StringToInt(splitted_str[1], &record_id));
    next_record_unique_id = std::max(next_record_unique_id, record_id + 1);
  }
  return next_record_unique_id;
}

}  // namespace

FakeBiodClient::FakeRecord::FakeRecord() = default;
FakeBiodClient::FakeRecord::FakeRecord(const FakeRecord&) = default;
FakeBiodClient::FakeRecord::~FakeRecord() = default;

void FakeBiodClient::FakeRecord::Clear() {
  user_id.clear();
  label.clear();
  fake_fingerprint.clear();
}

FakeBiodClient::FakeBiodClient() {
  CHECK(!g_instance);
  g_instance = this;
}

FakeBiodClient::~FakeBiodClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeBiodClient* FakeBiodClient::Get() {
  return g_instance;
}

void FakeBiodClient::SendRestarted() {
  current_session_ = FingerprintSession::NONE;

  for (auto& observer : observers_)
    observer.BiodServiceRestarted();
}

void FakeBiodClient::SendStatusChanged(biod::BiometricsManagerStatus status) {
  current_session_ = FingerprintSession::NONE;

  for (auto& observer : observers_) {
    observer.BiodServiceStatusChanged(status);
  }
}

void FakeBiodClient::SendEnrollScanDone(const std::string& fingerprint,
                                        biod::ScanResult type_result,
                                        bool is_complete,
                                        int percent_complete) {
  // Enroll scan signals do nothing if an enroll session is not happening.
  if (current_session_ != FingerprintSession::ENROLL)
    return;

  // The fake fingerprint gets appended to the current fake fingerprints.
  current_record_.fake_fingerprint.push_back(fingerprint);

  // If the enroll is complete, save the record and exit enroll mode.
  if (is_complete) {
    records_[current_record_path_] = std::move(current_record_);
    SaveRecords();
    current_record_path_ = dbus::ObjectPath();
    current_record_.Clear();
    current_session_ = FingerprintSession::NONE;
  }

  for (auto& observer : observers_)
    observer.BiodEnrollScanDoneReceived(type_result, is_complete,
                                        percent_complete);
}

void FakeBiodClient::SendAuthScanDone(const std::string& fingerprint,
                                      const biod::FingerprintMessage& msg) {
  // Auth scan signals do nothing if an auth session is not happening.
  if (current_session_ != FingerprintSession::AUTH)
    return;

  AuthScanMatches matches;
  if (msg.msg_case() == biod::FingerprintMessage::MsgCase::kScanResult &&
      msg.scan_result() == biod::ScanResult::SCAN_RESULT_SUCCESS) {
    // Iterate through all the records to check if fingerprint is a match and
    // populate |matches| accordingly. This searches through all the records and
    // then each record's fake fingerprint, but neither of these should ever
    // have more than five entries.
    for (const auto& entry : records_) {
      const FakeRecord& record = entry.second;
      if (base::Contains(record.fake_fingerprint, fingerprint)) {
        const std::string& user_id = record.user_id;
        matches[user_id].push_back(entry.first);
      }
    }
  }

  for (auto& observer : observers_)
    observer.BiodAuthScanDoneReceived(msg, matches);
}

void FakeBiodClient::SendSessionFailed() {
  if (current_session_ == FingerprintSession::NONE)
    return;

  for (auto& observer : observers_)
    observer.BiodSessionFailedReceived();
}

void FakeBiodClient::Reset() {
  records_.clear();
  current_record_.Clear();
  current_record_path_ = dbus::ObjectPath();
  current_session_ = FingerprintSession::NONE;
}

void FakeBiodClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBiodClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FakeBiodClient::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

void FakeBiodClient::StartEnrollSession(const std::string& user_id,
                                        const std::string& label,
                                        chromeos::ObjectPathCallback callback) {
  CHECK_EQ(current_session_, FingerprintSession::NONE);
  current_enroll_percentage_ = 0;

  // Create the enrollment with |user_id|, |label| and a empty fake fingerprint.
  current_record_path_ = dbus::ObjectPath(
      kRecordObjectPathPrefix + base::NumberToString(next_record_unique_id_++));
  current_record_.user_id = user_id;
  current_record_.label = label;
  current_session_ = FingerprintSession::ENROLL;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                dbus::ObjectPath(kEnrollSessionObjectPath)));
}

void FakeBiodClient::SetFakeUserDataDir(const base::FilePath& path) {
  fake_biod_db_filepath_ = path.Append("fake_biod");
  LoadRecords();
}

void FakeBiodClient::GetRecordsForUser(const std::string& user_id,
                                       UserRecordsCallback callback) {
  std::vector<dbus::ObjectPath> records_object_paths;
  for (const auto& record : records_) {
    if (record.second.user_id == user_id) {
      records_object_paths.push_back(record.first);
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), records_object_paths, true));
}

void FakeBiodClient::DestroyAllRecords(
    chromeos::VoidDBusMethodCallback callback) {
  records_.clear();
  SaveRecords();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeBiodClient::StartAuthSession(chromeos::ObjectPathCallback callback) {
  CHECK_EQ(current_session_, FingerprintSession::NONE);

  current_session_ = FingerprintSession::AUTH;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                dbus::ObjectPath(kAuthSessionObjectPath)));
}

void FakeBiodClient::RequestType(BiometricTypeCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), biod::BIOMETRIC_TYPE_FINGERPRINT));
}

void FakeBiodClient::CancelEnrollSession(
    chromeos::VoidDBusMethodCallback callback) {
  CHECK_EQ(current_session_, FingerprintSession::ENROLL);
  current_enroll_percentage_ = 0;

  // Clean up the in progress enrollment.
  current_record_.Clear();
  current_record_path_ = dbus::ObjectPath();
  current_session_ = FingerprintSession::NONE;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeBiodClient::EndAuthSession(chromeos::VoidDBusMethodCallback callback) {
  current_session_ = FingerprintSession::NONE;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeBiodClient::SetRecordLabel(const dbus::ObjectPath& record_path,
                                    const std::string& label,
                                    chromeos::VoidDBusMethodCallback callback) {
  auto it = records_.find(record_path);
  if (it != records_.end()) {
    it->second.label = label;
  }
  SaveRecords();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeBiodClient::RemoveRecord(const dbus::ObjectPath& record_path,
                                  chromeos::VoidDBusMethodCallback callback) {
  records_.erase(record_path);
  SaveRecords();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeBiodClient::RequestRecordLabel(const dbus::ObjectPath& record_path,
                                        LabelCallback callback) {
  std::string record_label;
  auto it = records_.find(record_path);
  if (it != records_.end()) {
    record_label = it->second.label;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), record_label));
}

void FakeBiodClient::TouchFingerprintSensor(int finger_id) {
  CHECK(finger_id > 0 && finger_id < 4);
  switch (current_session_) {
    case FingerprintSession::NONE:
      break;
    case FingerprintSession::AUTH: {
      biod::FingerprintMessage auth_result;
      auth_result.set_scan_result(biod::ScanResult::SCAN_RESULT_SUCCESS);
      SendAuthScanDone(base::NumberToString(finger_id), auth_result);
      break;
    }
    case FingerprintSession::ENROLL:
      current_enroll_percentage_ += 100 / finger_id;
      current_enroll_percentage_ = std::min(100, current_enroll_percentage_);
      SendEnrollScanDone(base::NumberToString(finger_id),
                         biod::ScanResult::SCAN_RESULT_SUCCESS,
                         current_enroll_percentage_ == 100,
                         current_enroll_percentage_);
      break;
  }
}

void FakeBiodClient::SaveRecords() const {
  base::ScopedAllowBlockingForTesting allow_io;
  if (records_.empty()) {
    base::DeleteFile(fake_biod_db_filepath_);
    return;
  }
  const base::Value::Dict& record_dict = FakeRecordsToValue(records_);
  if (auto json_string = base::WriteJson(record_dict)) {
    if (base::WriteFile(fake_biod_db_filepath_, json_string.value())) {
      return;
    }
  }
  LOG(ERROR) << "FakeBiod SaveRecords failed.";
}

void FakeBiodClient::LoadRecords() {
  CHECK(records_.empty());
  base::ScopedAllowBlockingForTesting allow_io;
  if (!base::PathExists(fake_biod_db_filepath_)) {
    return;
  }
  std::string content;
  if (!base::ReadFileToString(fake_biod_db_filepath_, &content)) {
    LOG(ERROR) << "FakeBiod failed to read the file: "
               << fake_biod_db_filepath_;
    return;
  }
  std::optional<base::Value> records_json = base::JSONReader::Read(content);
  if (!records_json.has_value()) {
    LOG(ERROR) << "FakeBiod parse failed.";
    return;
  }

  records_ = ValueToFakeRecords(records_json.value());
  next_record_unique_id_ = GetNextRecordId(records_);
}

}  // namespace ash
