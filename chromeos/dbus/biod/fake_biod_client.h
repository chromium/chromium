// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BIOD_FAKE_BIOD_CLIENT_H_
#define CHROMEOS_DBUS_BIOD_FAKE_BIOD_CLIENT_H_

#include <map>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/dbus/biod/biod_client.h"
#include "dbus/object_path.h"

namespace chromeos {

// A fake implementation of BiodClient. It emulates the real Biod daemon by
// providing the same API and storing fingerprints locally. A fingerprint is
// represented by a vector of strings. During enrollment, fake enrollments are
// sent as strings. If they are successful they get added to the current
// fingerprint, until a completed enroll scan is sent. An attempt scan is also
// sent with a string. If that string matches any string in the stored
// fingerprint vector, it is considered a match.
class COMPONENT_EXPORT(BIOD_CLIENT) FakeBiodClient : public BiodClient {
 public:
  FakeBiodClient();
  ~FakeBiodClient() override;

  // Checks that a FakeBiodClient instance was initialized and returns it.
  static FakeBiodClient* Get();

  // Emulates the biod daemon by sending events which the daemon normally sends.
  // Notifies |observers_| about various events. These will be used in tests.

  // Emulates a scan that occurs during enrolling a new fingerprint.
  // |fingerprint| is the fake data of the finger as a string. If |is_complete|
  // is true the enroll session is finished, and the record is stored.
  void SendEnrollScanDone(const std::string& fingerprint,
                          biod::ScanResult type_result,
                          bool is_complete,
                          int percent_complete);
  // Emulates a scan that occurs during a authentication session. |fingerprint|
  // is a string which represents the finger, and will be compared with all the
  // stored fingerprints.
  void SendAuthScanDone(const std::string& fingerprint,
                        biod::ScanResult type_result);
  void SendSessionFailed();

  // Clears all stored and current records from the fake storage.
  void Reset();

  // BiodClient:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(const Observer* observer) const override;
  void StartEnrollSession(const std::string& user_id,
                          const std::string& label,
                          const ObjectPathCallback& callback) override;
  void GetRecordsForUser(const std::string& user_id,
                         UserRecordsCallback callback) override;
  void DestroyAllRecords(VoidDBusMethodCallback callback) override;
  void StartAuthSession(const ObjectPathCallback& callback) override;
  void RequestType(BiometricTypeCallback callback) override;
  void CancelEnrollSession(VoidDBusMethodCallback callback) override;
  void EndAuthSession(VoidDBusMethodCallback callback) override;
  void SetRecordLabel(const dbus::ObjectPath& record_path,
                      const std::string& label,
                      VoidDBusMethodCallback callback) override;
  void RemoveRecord(const dbus::ObjectPath& record_path,
                    VoidDBusMethodCallback callback) override;
  void RequestRecordLabel(const dbus::ObjectPath& record_path,
                          LabelCallback callback) override;

 private:
  struct FakeRecord;

  // The current session of fingerprint storage. The session determines which
  // events will be sent from user finger touches.
  enum class FingerprintSession {
    NONE,
    ENROLL,
    AUTH,
  };

  // The stored fingerprints.
  std::map<dbus::ObjectPath, std::unique_ptr<FakeRecord>> records_;

  // Current record in process of getting enrolled and its path. These are
  // assigned at the start of an enroll session and freed when the enroll
  // session is finished or cancelled.
  dbus::ObjectPath current_record_path_;
  std::unique_ptr<FakeRecord> current_record_;

  // Unique indentifier that gets updated each time an enroll session is started
  // to ensure each record is stored at a different path.
  int next_record_unique_id_ = 1;

  // The current session of the fake storage.
  FingerprintSession current_session_ = FingerprintSession::NONE;

  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(FakeBiodClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_BIOD_FAKE_BIOD_CLIENT_H_
