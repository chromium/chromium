// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/esim_manager.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/hermes/hermes_clients.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom-forward.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "dbus/object_path.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace chromeos {
namespace cellular_setup {

namespace {
const char* kTestEuiccPath = "/org/chromium/Hermes/Euicc/0";
const char* kTestEid = "12345678901234567890123456789012";

}  // namespace

// Fake observer for testing ESimManager.
class ESimManagerTestObserver : public mojom::ESimManagerObserver {
 public:
  ESimManagerTestObserver() = default;
  ESimManagerTestObserver(const ESimManagerTestObserver&) = delete;
  ESimManagerTestObserver& operator=(const ESimManagerTestObserver&) = delete;
  ~ESimManagerTestObserver() override = default;

  // mojom::ESimManagerObserver:
  void OnAvailableEuiccListChanged() override {
    available_euicc_list_change_count_++;
  }
  void OnProfileListChanged(mojo::PendingRemote<mojom::Euicc> euicc) override {
    profile_list_change_calls_.push_back(std::move(euicc));
  }
  void OnEuiccChanged(mojo::PendingRemote<mojom::Euicc> euicc) override {
    euicc_change_calls_.push_back(std::move(euicc));
  }
  void OnProfileChanged(
      mojo::PendingRemote<mojom::ESimProfile> esim_profile) override {
    profile_change_calls_.push_back(std::move(esim_profile));
  }

  mojo::PendingRemote<mojom::ESimManagerObserver> GenerateRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void Reset() {
    available_euicc_list_change_count_ = 0;
    profile_list_change_calls_.clear();
    euicc_change_calls_.clear();
    profile_change_calls_.clear();
  }

  mojo::PendingRemote<mojom::Euicc> PopLastChangedEuicc() {
    mojo::PendingRemote<mojom::Euicc> euicc =
        std::move(euicc_change_calls_.front());
    euicc_change_calls_.erase(euicc_change_calls_.begin());
    return euicc;
  }

  mojo::PendingRemote<mojom::ESimProfile> PopLastChangedESimProfile() {
    mojo::PendingRemote<mojom::ESimProfile> esim_profile =
        std::move(profile_change_calls_.front());
    profile_change_calls_.erase(profile_change_calls_.begin());
    return esim_profile;
  }

  int available_euicc_list_change_count() {
    return available_euicc_list_change_count_;
  }

  const std::vector<mojo::PendingRemote<mojom::Euicc>>&
  profile_list_change_calls() {
    return profile_list_change_calls_;
  }

  const std::vector<mojo::PendingRemote<mojom::Euicc>>& euicc_change_calls() {
    return euicc_change_calls_;
  }

  const std::vector<mojo::PendingRemote<mojom::ESimProfile>>&
  profile_change_calls() {
    return profile_change_calls_;
  }

 private:
  int available_euicc_list_change_count_ = 0;
  std::vector<mojo::PendingRemote<mojom::Euicc>> profile_list_change_calls_;
  std::vector<mojo::PendingRemote<mojom::Euicc>> euicc_change_calls_;
  std::vector<mojo::PendingRemote<mojom::ESimProfile>> profile_change_calls_;
  mojo::Receiver<mojom::ESimManagerObserver> receiver_{this};
};

class ESimManagerTest : public testing::Test {
 public:
  using InstallResultPair =
      std::pair<mojom::ProfileInstallResult, mojom::ESimProfilePtr>;

  ESimManagerTest() {
    if (!ShillManagerClient::Get())
      shill_clients::InitializeFakes();
    if (!HermesManagerClient::Get())
      hermes_clients::InitializeFakes();
  }
  ESimManagerTest(const ESimManagerTest&) = delete;
  ESimManagerTest& operator=(const ESimManagerTest&) = delete;
  ~ESimManagerTest() override = default;

  void SetUp() override {
    HermesManagerClient::Get()->GetTestInterface()->ClearEuiccs();
    HermesEuiccClient::Get()->GetTestInterface()->SetInteractiveDelay(
        base::TimeDelta::FromSeconds(0));
    esim_manager_ = std::make_unique<ESimManager>();
    observer_ = std::make_unique<ESimManagerTestObserver>();
    esim_manager_->AddObserver(observer_->GenerateRemote());
  }

  void TearDown() override {
    esim_manager_.reset();
    observer_.reset();
    HermesEuiccClient::Get()->GetTestInterface()->ResetPendingEventsRequested();
  }

  void SetupEuicc() {
    HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
        dbus::ObjectPath(kTestEuiccPath), kTestEid, true);
    base::RunLoop().RunUntilIdle();
  }

  std::vector<mojo::PendingRemote<mojom::Euicc>> GetAvailableEuiccs() {
    std::vector<mojo::PendingRemote<mojom::Euicc>> result;
    base::RunLoop run_loop;
    esim_manager_->GetAvailableEuiccs(base::BindOnce(
        [](std::vector<mojo::PendingRemote<mojom::Euicc>>* result,
           base::OnceClosure quit_closure,
           std::vector<mojo::PendingRemote<mojom::Euicc>> available_euiccs) {
          for (auto& euicc : available_euiccs)
            result->push_back(std::move(euicc));
          std::move(quit_closure).Run();
        },
        &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  mojom::EuiccPropertiesPtr GetEuiccProperties(
      const mojo::Remote<mojom::Euicc>& euicc) {
    mojom::EuiccPropertiesPtr result;
    base::RunLoop run_loop;
    euicc->GetProperties(base::BindOnce(
        [](mojom::EuiccPropertiesPtr* out, base::OnceClosure quit_closure,
           mojom::EuiccPropertiesPtr properties) {
          *out = std::move(properties);
          std::move(quit_closure).Run();
        },
        &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  mojom::ESimProfilePropertiesPtr GetESimProfileProperties(
      const mojo::Remote<mojom::ESimProfile>& esim_profile) {
    mojom::ESimProfilePropertiesPtr result;
    base::RunLoop run_loop;
    esim_profile->GetProperties(base::BindOnce(
        [](mojom::ESimProfilePropertiesPtr* out, base::OnceClosure quit_closure,
           mojom::ESimProfilePropertiesPtr properties) {
          *out = std::move(properties);
          std::move(quit_closure).Run();
        },
        &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  ESimManager* esim_manager() { return esim_manager_.get(); }
  ESimManagerTestObserver* observer() { return observer_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<ESimManager> esim_manager_;
  std::unique_ptr<ESimManagerTestObserver> observer_;
};

TEST_F(ESimManagerTest, GetAvailableEuiccs) {
  ASSERT_EQ(0u, GetAvailableEuiccs().size());
  SetupEuicc();
  // Verify that GetAvailableEuiccs call returns list of euiccs.
  std::vector<mojo::PendingRemote<mojom::Euicc>> available_euiccs =
      GetAvailableEuiccs();
  ASSERT_EQ(1u, available_euiccs.size());
  mojo::Remote<mojom::Euicc> euicc(std::move(available_euiccs.front()));
  mojom::EuiccPropertiesPtr properties = GetEuiccProperties(euicc);
  EXPECT_EQ(kTestEid, properties->eid);
}

TEST_F(ESimManagerTest, ListChangeNotification) {
  SetupEuicc();
  // Verify that available euicc list change is notified.
  ASSERT_EQ(1, observer()->available_euicc_list_change_count());

  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath active_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kActive, "");
  dbus::ObjectPath pending_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kPending, "");
  base::RunLoop().RunUntilIdle();
  // Verify the profile list change is notified to observer.
  ASSERT_EQ(2u, observer()->profile_list_change_calls().size());
}

TEST_F(ESimManagerTest, EuiccChangeNotification) {
  SetupEuicc();
  HermesEuiccClient::Properties* dbus_properties =
      HermesEuiccClient::Get()->GetProperties(dbus::ObjectPath(kTestEuiccPath));
  dbus_properties->is_active().ReplaceValue(false);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, observer()->euicc_change_calls().size());
  mojo::Remote<mojom::Euicc> euicc(observer()->PopLastChangedEuicc());
  mojom::EuiccPropertiesPtr mojo_properties = GetEuiccProperties(euicc);
  EXPECT_EQ(kTestEid, mojo_properties->eid);
}

TEST_F(ESimManagerTest, ESimProfileChangeNotification) {
  SetupEuicc();
  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::kActive, "");
  base::RunLoop().RunUntilIdle();

  HermesProfileClient::Properties* dbus_properties =
      HermesProfileClient::Get()->GetProperties(dbus::ObjectPath(profile_path));
  dbus_properties->state().ReplaceValue(hermes::profile::kInactive);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, observer()->profile_change_calls().size());
  mojo::Remote<mojom::ESimProfile> esim_profile(
      observer()->PopLastChangedESimProfile());
  mojom::ESimProfilePropertiesPtr mojo_properties =
      GetESimProfileProperties(esim_profile);
  EXPECT_EQ(dbus_properties->iccid().value(), mojo_properties->iccid);
}

}  // namespace cellular_setup
}  // namespace chromeos
