// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_helpers.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/arc/session/arc_container_client_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcContainerClientAdapterTest : public testing::Test {
 public:
  ArcContainerClientAdapterTest() = default;
  ~ArcContainerClientAdapterTest() override = default;
  ArcContainerClientAdapterTest(const ArcContainerClientAdapterTest&) = delete;
  ArcContainerClientAdapterTest& operator=(
      const ArcContainerClientAdapterTest&) = delete;

  void SetUp() override {
    chromeos::SessionManagerClient::InitializeFake();
    client_adapter_ = CreateArcContainerClientAdapter();
  }

  void TearDown() override {
    client_adapter_ = nullptr;
    chromeos::SessionManagerClient::Shutdown();
  }

 protected:
  ArcClientAdapter* client_adapter() { return client_adapter_.get(); }

 private:
  std::unique_ptr<ArcClientAdapter> client_adapter_;
};

// b/164816080 This test ensures that a new container instance that is
// created while handling the shutting down of the previous instance,
// doesn't incorrectly receive the shutdown event as well.
TEST_F(ArcContainerClientAdapterTest,
       DoesNotGetArcInstanceStoppedOnNestedInstance) {
  class Observer : public ArcClientAdapter::Observer {
   public:
    explicit Observer(Observer* child_observer)
        : child_observer_(child_observer) {}
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    ~Observer() override {
      if (child_observer_ && nested_client_adapter_)
        nested_client_adapter_->RemoveObserver(child_observer_);
    }

    bool stopped_called() const { return stopped_called_; }

    // ArcClientAdapter::Observer:
    void ArcInstanceStopped() override {
      stopped_called_ = true;

      if (child_observer_) {
        nested_client_adapter_ = CreateArcContainerClientAdapter();
        nested_client_adapter_->AddObserver(child_observer_);
      }
    }

   private:
    Observer* const child_observer_;
    std::unique_ptr<ArcClientAdapter> nested_client_adapter_;
    bool stopped_called_ = false;
  };

  Observer child_observer(nullptr);
  Observer parent_observer(&child_observer);
  client_adapter()->AddObserver(&parent_observer);
  base::ScopedClosureRunner teardown(base::BindOnce(
      [](ArcClientAdapter* client_adapter, Observer* parent_observer) {
        client_adapter->RemoveObserver(parent_observer);
      },
      client_adapter(), &parent_observer));

  chromeos::FakeSessionManagerClient::Get()->NotifyArcInstanceStopped();

  EXPECT_TRUE(parent_observer.stopped_called());
  EXPECT_FALSE(child_observer.stopped_called());
}

}  // namespace arc
