// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_BIOD_FAKE_BIOD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_BIOD_FAKE_BIOD_CLIENT_H_

#include <map>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/biod/biod_client.h"
#include "dbus/object_path.h"

namespace ash {

// A fake implementation of BiodClient. It emulates the real Biod daemon by
// providing the same API and storing fingerprints locally. A fingerprint is
// represented by a vector of strings. During enrollment, fake enrollments are
// sent as strings. If they are successful they get added to the current
// fingerprint, until a completed enroll scan is sent. An attempt scan is also
// sent with a string. If that string matches any string in the stored
// fingerprint vector, it is considered a match.
class COMPONENT_EXPORT(BIOD_CLIENT) FakeBiodClient : public BiodClient {
 public:
  struct FakeRecord {
    FakeRecord();
    FakeRecord(const FakeRecord&);
    ~FakeRecord();

    void Clear();

    std::string user_id;
    std::string label;
    // A fake fingerprint is a vector which consists of all the strings which
    // were "pressed" during the enroll session.
    std::vector<std::string> fake_fingerprint;
  };

  using RecordMap = std::map<dbus::ObjectPath, FakeRecord>;

  FakeBiodClient();

  FakeBiodClient(const FakeBiodClient&) = delete;
  FakeBiodClient& operator=(const FakeBiodClient&) = delete;

  ~FakeBiodClient() override;

  // Checks that a FakeBiodClient instance was initialized and returns it.
  static FakeBiodClient* Get();

  // Emulates the biod daemon by sending events which the daemon normally sends.
  // Notifies |observers_| about various events. These will be used in tests.

  // Emulates biod service restarted.
  void SendRestarted();

  // Emulates biod service status changed.
  void SendStatusChanged(biod::BiometricsManagerStatus status);

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
                        const biod::FingerprintMessage& msg);
  void SendSessionFailed();

  // Clears all stored and current records from the fake storage.
  void Reset();

  // BiodClient:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(const Observer* observer) const override;
  void StartEnrollSession(const std::string& user_id,
                          const std::string& label,
                          chromeos::ObjectPathCallback callback) override;
  void GetRecordsForUser(const std::string& user_id,
                         UserRecordsCallback callback) override;
  void DestroyAllRecords(chromeos::VoidDBusMethodCallback callback) override;
  void StartAuthSession(chromeos::ObjectPathCallback callback) override;
  void RequestType(BiometricTypeCallback callback) override;
  void CancelEnrollSession(chromeos::VoidDBusMethodCallback callback) override;
  void EndAuthSession(chromeos::VoidDBusMethodCallback callback) override;
  void SetRecordLabel(const dbus::ObjectPath& record_path,
                      const std::string& label,
                      chromeos::VoidDBusMethodCallback callback) override;
  void RemoveRecord(const dbus::ObjectPath& record_path,
                    chromeos::VoidDBusMethodCallback callback) override;
  void RequestRecordLabel(const dbus::ObjectPath& record_path,
                          LabelCallback callback) override;

  // Simulates the finger_id finger touches the sensor.
  void TouchFingerprintSensor(int finger_id);

  // Sets the directory where the fake records will be saved.
  void SetFakeUserDataDir(const base::FilePath& path);

 private:
  // The current session of fingerprint storage. The session determines which
  // events will be sent from user finger touches.
  enum class FingerprintSession {
    NONE,
    ENROLL,
    AUTH,
  };

  // Save the fake fingerprint FakeRecord's to the user data directory.
  void SaveRecords() const;
  // Load the fake fingerprint FakeRecord's from the user data directory.
  void LoadRecords();

  // The current enrollment session progress percentage.
  int current_enroll_percentage_ = 0;

  // The stored fingerprints.
  RecordMap records_;

  // Current record in process of getting enrolled and its path. These are
  // assigned at the start of an enroll session and freed when the enroll
  // session is finished or cancelled.
  dbus::ObjectPath current_record_path_;
  FakeRecord current_record_;

  // Unique indentifier that gets updated each time an enroll session is started
  // to ensure each record is stored at a different path.
  int next_record_unique_id_ = 1;

  // The current session of the fake storage.
  FingerprintSession current_session_ = FingerprintSession::NONE;

  // Stores the saved fingerprint records path.
  base::FilePath fake_biod_db_filepath_;

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_BIOD_FAKE_BIOD_CLIENT_H_
