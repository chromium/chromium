// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_BIOD_BIOD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_BIOD_BIOD_CLIENT_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/biod/constants.pb.h"
#include "chromeos/ash/components/dbus/biod/messages.pb.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace dbus {
class Bus;
}

namespace ash {

// Each time the sensor detects a scan, an object containing all the users, each
// with the object paths of all the matched stored biometrics is returned. The
// users are unique identifiers which are assigned by Chrome. The object path
// represent the object path of the biometric.
using AuthScanMatches =
    std::unordered_map<std::string, std::vector<dbus::ObjectPath>>;

// BiodClient is used to communicate with a biod D-Bus manager
// interface.
class COMPONENT_EXPORT(BIOD_CLIENT) BiodClient {
 public:
  // Interface for observing changes from the biometrics manager.
  class Observer {
   public:
    // Called when biometrics manager powers up or is restarted.
    virtual void BiodServiceRestarted() {}

    // Called when biometrics manager status changed: e.g initialized.
    virtual void BiodServiceStatusChanged(
        biod::BiometricsManagerStatus status) {}

    // Called whenever a user attempts a scan during enrollment. |scan_result|
    // tells whether the scan was succesful. |enroll_session_complete| tells
    // whether enroll session is complete and is now over.
    // |percent_complete| within [0, 100] represents the percent of enrollment
    // completion and -1 means unknown percentage.
    virtual void BiodEnrollScanDoneReceived(biod::ScanResult scan_result,
                                            bool enroll_session_complete,
                                            int percent_complete) {}

    // Called when an authentication scan is performed. If the scan is
    // successful, |matches| will equal all the enrollment IDs that match the
    // scan, and the labels of the matched fingerprints.
    virtual void BiodAuthScanDoneReceived(const biod::FingerprintMessage& msg,
                                          const AuthScanMatches& matches) {}

    // Called during an enrollment or authentication session to indicate a
    // failure. Any enrollment that was underway is thrown away and auth will
    // no longer be happening.
    virtual void BiodSessionFailedReceived() {}

   protected:
    virtual ~Observer() {}
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static BiodClient* Get();

  BiodClient(const BiodClient&) = delete;
  BiodClient& operator=(const BiodClient&) = delete;

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns true if this object has the given observer.
  virtual bool HasObserver(const Observer* observer) const = 0;

  // UserRecordsCallback is used for the GetRecordsForUser method. It receives
  // one argument which contains a list of the stored records' object paths for
  // a given user.
  using UserRecordsCallback =
      base::OnceCallback<void(const std::vector<dbus::ObjectPath>&, bool)>;

  // BiometricTypeCallback is used for the GetType method. It receives
  // one argument which states the type of biometric.
  using BiometricTypeCallback = base::OnceCallback<void(biod::BiometricType)>;

  // LabelCallback is for the RequestRecordLabel method.
  using LabelCallback = base::OnceCallback<void(const std::string& label)>;

  // Starts the biometric enroll session. |callback| is called with the object
  // path of the current enroll session after the method succeeds. |user_id|
  // contains the unique identifier for the owner of the biometric. |label| is
  // the the human readable label the user gave the biometric.
  virtual void StartEnrollSession(const std::string& user_id,
                                  const std::string& label,
                                  chromeos::ObjectPathCallback callback) = 0;

  // Gets all the records registered with this biometric. |callback| is called
  // with all the object paths of the records with |user_id| after this method
  // succeeds. |user_id| contains the unique identifier for the owner of the
  // biometric.
  virtual void GetRecordsForUser(const std::string& user_id,
                                 UserRecordsCallback callback) = 0;

  // Irreversibly destroys all records registered. |callback| is called
  // asynchronously with the result.
  virtual void DestroyAllRecords(chromeos::VoidDBusMethodCallback callback) = 0;

  // Starts the biometric auth session. |callback| is called with the object
  // path of the auth session after the method succeeds.
  virtual void StartAuthSession(chromeos::ObjectPathCallback callback) = 0;

  // Requests the type of biometric. |callback| is called with the biometric
  // type after the method succeeds.
  virtual void RequestType(BiometricTypeCallback callback) = 0;

  // Cancels the enroll session.
  // |callback| is called asynchronously with the result.
  virtual void CancelEnrollSession(
      chromeos::VoidDBusMethodCallback callback) = 0;

  // Ends the auth session.
  // |callback| is called asynchronously with the result.
  virtual void EndAuthSession(chromeos::VoidDBusMethodCallback callback) = 0;

  // Changes the label of the record at |record_path| to |label|. |callback| is
  // called asynchronously with the result.
  virtual void SetRecordLabel(const dbus::ObjectPath& record_path,
                              const std::string& label,
                              chromeos::VoidDBusMethodCallback callback) = 0;

  // Removes the record at |record_path|. This record will no longer be able to
  // used for authentication. |callback| is called asynchronously with the
  // result.
  virtual void RemoveRecord(const dbus::ObjectPath& record_path,
                            chromeos::VoidDBusMethodCallback callback) = 0;

  // Requests the label of the record at |record_path|. |callback| is called
  // with the label of the record.
  virtual void RequestRecordLabel(const dbus::ObjectPath& record_path,
                                  LabelCallback callback) = 0;

 protected:
  friend class BiodClientTest;

  // Initialize/Shutdown should be used instead.
  BiodClient();
  virtual ~BiodClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_BIOD_BIOD_CLIENT_H_
