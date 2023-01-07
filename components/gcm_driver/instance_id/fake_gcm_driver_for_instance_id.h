// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_INSTANCE_ID_FAKE_GCM_DRIVER_FOR_INSTANCE_ID_H_
#define COMPONENTS_GCM_DRIVER_INSTANCE_ID_FAKE_GCM_DRIVER_FOR_INSTANCE_ID_H_

#include <map>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "components/gcm_driver/fake_gcm_driver.h"

namespace base {
class FilePath;
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

  virtual std::string GenerateTokenImpl(const std::string& app_id,
                                        const std::string& authorized_entity,
                                        const std::string& scope);

 private:
  std::map<std::string, std::pair<std::string, std::string>> instance_id_data_;
  std::map<std::string, std::string> tokens_;
  std::string last_gettoken_app_id_;
  std::string last_gettoken_authorized_entity_;
  std::string last_deletetoken_app_id_;
};

}  // namespace instance_id

#endif  // COMPONENTS_GCM_DRIVER_INSTANCE_ID_FAKE_GCM_DRIVER_FOR_INSTANCE_ID_H_
