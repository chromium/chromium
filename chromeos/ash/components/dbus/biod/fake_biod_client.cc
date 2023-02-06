// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/biod/fake_biod_client.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
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

}  // namespace

// FakeRecord is the definition of a fake stored fingerprint template.
struct FakeBiodClient::FakeRecord {
  std::string user_id;
  std::string label;
  // A fake fingerprint is a vector which consists of all the strings which
  // were "pressed" during the enroll session.
  std::vector<std::string> fake_fingerprint;
};

FakeBiodClient::FakeBiodClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakeBiodClient::~FakeBiodClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeBiodClient* FakeBiodClient::Get() {
  DCHECK(g_instance);
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

  DCHECK(current_record_);
  // The fake fingerprint gets appended to the current fake fingerprints.
  current_record_->fake_fingerprint.push_back(fingerprint);

  // If the enroll is complete, save the record and exit enroll mode.
  if (is_complete) {
    records_[current_record_path_] = std::move(current_record_);
    current_record_path_ = dbus::ObjectPath();
    current_record_.reset();
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
      const std::unique_ptr<FakeRecord>& record = entry.second;
      if (base::Contains(record->fake_fingerprint, fingerprint)) {
        const std::string& user_id = record->user_id;
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
  current_record_.reset();
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
  DCHECK_EQ(current_session_, FingerprintSession::NONE);

  // Create the enrollment with |user_id|, |label| and a empty fake fingerprint.
  current_record_path_ = dbus::ObjectPath(
      kRecordObjectPathPrefix + std::to_string(next_record_unique_id_++));
  current_record_ = std::make_unique<FakeRecord>();
  current_record_->user_id = user_id;
  current_record_->label = label;
  current_session_ = FingerprintSession::ENROLL;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                dbus::ObjectPath(kEnrollSessionObjectPath)));
}

void FakeBiodClient::GetRecordsForUser(const std::string& user_id,
                                       UserRecordsCallback callback) {
  std::vector<dbus::ObjectPath> records_object_paths;
  for (const auto& record : records_) {
    if (record.second->user_id == user_id)
      records_object_paths.push_back(record.first);
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), records_object_paths));
}

void FakeBiodClient::DestroyAllRecords(
    chromeos::VoidDBusMethodCallback callback) {
  records_.clear();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeBiodClient::StartAuthSession(chromeos::ObjectPathCallback callback) {
  DCHECK_EQ(current_session_, FingerprintSession::NONE);

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
  DCHECK_EQ(current_session_, FingerprintSession::ENROLL);

  // Clean up the in progress enrollment.
  current_record_.reset();
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
  if (records_.find(record_path) != records_.end())
    records_[record_path]->label = label;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeBiodClient::RemoveRecord(const dbus::ObjectPath& record_path,
                                  chromeos::VoidDBusMethodCallback callback) {
  records_.erase(record_path);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeBiodClient::RequestRecordLabel(const dbus::ObjectPath& record_path,
                                        LabelCallback callback) {
  std::string record_label;
  if (records_.find(record_path) != records_.end())
    record_label = records_[record_path]->label;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), record_label));
}

}  // namespace ash
