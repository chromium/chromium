// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_INSTANCE_ID_FAKE_GCM_DRIVER_FOR_INSTANCE_ID_H_
#define COMPONENTS_GCM_DRIVER_INSTANCE_ID_FAKE_GCM_DRIVER_FOR_INSTANCE_ID_H_

#include <map>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/gcm_driver/fake_gcm_driver.h"

namespace base {
class SequencedTaskRunner;
}

namespace instance_id {

class FakeGCMDriverForInstanceID : public gcm::FakeGCMDriver,
                                   protected gcm::InstanceIDHandler {
 public:
  FakeGCMDriverForInstanceID();
  explicit FakeGCMDriverForInstanceID(
      const base::FilePath& store_path,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);

  FakeGCMDriverForInstanceID(const FakeGCMDriverForInstanceID&) = delete;
  FakeGCMDriverForInstanceID& operator=(const FakeGCMDriverForInstanceID&) =
      delete;

  ~FakeGCMDriverForInstanceID() override;

  // FakeGCMDriver overrides:
  gcm::InstanceIDHandler* GetInstanceIDHandlerInternal() override;
  void AddConnectionObserver(gcm::GCMConnectionObserver* observer) override;
  void RemoveConnectionObserver(gcm::GCMConnectionObserver* observer) override;
  void AddAppHandler(const std::string& app_id,
                     gcm::GCMAppHandler* handler) override;

  // Expose protected method for testing.
  using gcm::FakeGCMDriver::DispatchMessage;

  // Returns true if the given |app_id| has the expected |token|. Note that
  // tokens may be loaded before GCMDriver is connected.
  bool HasTokenForAppId(const std::string& app_id,
                        const std::string& token) const;

  // GCMDriver will not connect until the given |app_id| is added.
  void WaitForAppIdBeforeConnection(const std::string& app_id);

  const std::string& last_gettoken_app_id() const {
    return last_gettoken_app_id_;
  }
  const std::string& last_gettoken_authorized_entity() const {
    return last_gettoken_authorized_entity_;
  }
  const std::string& last_deletetoken_app_id() const {
    return last_deletetoken_app_id_;
  }

 protected:
  // InstanceIDHandler overrides:
  void GetToken(const std::string& app_id,
                const std::string& authorized_entity,
                const std::string& scope,
                base::TimeDelta time_to_live,
                GetTokenCallback callback) override;
  void ValidateToken(const std::string& app_id,
                     const std::string& authorized_entity,
                     const std::string& scope,
                     const std::string& token,
                     ValidateTokenCallback callback) override;
  void DeleteToken(const std::string& app_id,
                   const std::string& authorized_entity,
                   const std::string& scope,
                   DeleteTokenCallback callback) override;
  void AddInstanceIDData(const std::string& app_id,
                         const std::string& instance_id,
                         const std::string& extra_data) override;
  void RemoveInstanceIDData(const std::string& app_id) override;
  void GetInstanceIDData(const std::string& app_id,
                         GetInstanceIDDataCallback callback) override;

 private:
  // Stores generated FCM registration tokens to a file, to keep the same tokens
  // across browser restarts in tests.
  void StoreTokensIfNeeded();

  // Used to simulate connection of GCMDriver after adding the first
  // GCMAppHandler.
  void ConnectIfNeeded();

  std::string GenerateTokenImpl(const std::string& app_id,
                                const std::string& authorized_entity,
                                const std::string& scope);

  // Used to store FCM registration tokens across browser restarts in tests.
  const base::FilePath store_path_;

  std::map<std::string, std::pair<std::string, std::string>> instance_id_data_;
  std::map<std::string, std::string> tokens_;
  std::string last_gettoken_app_id_;
  std::string last_gettoken_authorized_entity_;
  std::string last_deletetoken_app_id_;

  // Simulate a connection to the server only after the given AppHandler has
  // been added. This is required to prevent message loss in GCMDriver while
  // dispatching a message.
  // TODO(crbug.com/40888673): remove once GCMDriver fixes it.
  std::string app_id_for_connection_;
  bool connected_ = false;

  base::ObserverList<gcm::GCMConnectionObserver,
                     /*check_empty=*/false,
                     /*allow_reentrancy=*/false>::Unchecked
      connection_observers_;

  base::WeakPtrFactory<FakeGCMDriverForInstanceID> weak_ptr_factory_{this};
};

}  // namespace instance_id

#endif  // COMPONENTS_GCM_DRIVER_INSTANCE_ID_FAKE_GCM_DRIVER_FOR_INSTANCE_ID_H_
