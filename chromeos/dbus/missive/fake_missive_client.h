// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MISSIVE_FAKE_MISSIVE_CLIENT_H_
#define CHROMEOS_DBUS_MISSIVE_FAKE_MISSIVE_CLIENT_H_

#include "base/callback_forward.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "chromeos/dbus/missive/missive_client.h"

namespace chromeos {

// Fake implementation of MissiveClient. This is currently a no-op fake.
class FakeMissiveClient : public MissiveClient {
 public:
  FakeMissiveClient();
  ~FakeMissiveClient() override;

  FakeMissiveClient(const FakeMissiveClient& other) = delete;
  FakeMissiveClient& operator=(const FakeMissiveClient& other) = delete;

  void Init();

 private:
  void AddRecord(
      const reporting::Priority priority,
      reporting::Record record,
      base::OnceCallback<void(reporting::Status)> completion_callback) override;

  void Flush(
      const reporting::Priority priority,
      base::OnceCallback<void(reporting::Status)> completion_callback) override;

  void ReportSuccess(
      const reporting::SequencingInformation& sequencing_information,
      bool force_confirm) override;

  void UpdateEncryptionKey(
      const reporting::SignedEncryptionInfo& encryption_info) override;

  base::WeakPtrFactory<FakeMissiveClient> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MISSIVE_FAKE_MISSIVE_CLIENT_H_
