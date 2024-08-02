// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/wayland/zaura_output_manager.h"

#include <sys/socket.h>
#include <cstdint>

#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/test/test_client.h"
#include "components/exo/wayland/test/wayland_server_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo::wayland {

namespace {

using ::testing::_;

class MockAuraOutputManagerListener {
 public:
  static void OnDone(void* data,
                     zaura_output_manager* output_manager,
                     wl_output* output) {
    auto* self = static_cast<MockAuraOutputManagerListener*>(data);
    self->MockOnDone(output);
  }
  MOCK_METHOD(void, MockOnDone, (wl_output * output));

  static void OnDisplayId(void* data,
                          zaura_output_manager* output_manager,
                          wl_output* output,
                          uint32_t display_id_hi,
                          uint32_t display_id_lo) {
    auto* self = static_cast<MockAuraOutputManagerListener*>(data);
    self->MockOnDisplayId(output, display_id_hi, display_id_lo);
  }
  MOCK_METHOD(void,
              MockOnDisplayId,
              (wl_output * output,
               uint32_t display_id_hi,
               uint32_t display_id_lo));

  static void OnLogicalPosition(void* data,
                                zaura_output_manager* output_manager,
                                wl_output* output,
                                int32_t x,
                                int32_t y) {
    auto* self = static_cast<MockAuraOutputManagerListener*>(data);
    self->MockOnLogicalPosition(output, x, y);
  }
  MOCK_METHOD(void,
              MockOnLogicalPosition,
              (wl_output * output, int32_t x, int32_t y));

  static void OnLogicalSize(void* data,
                            zaura_output_manager* output_manager,
                            wl_output* output,
                            int32_t width,
                            int32_t height) {
    auto* self = static_cast<MockAuraOutputManagerListener*>(data);
    self->MockOnLogicalSize(output, width, height);
  }
  MOCK_METHOD(void,
              MockOnLogicalSize,
              (wl_output * output, int32_t width, int32_t height));

  static void OnPhysicalSize(void* data,
                             zaura_output_manager* output_manager,
                             wl_output* output,
                             int32_t width,
                             int32_t height) {
    auto* self = static_cast<MockAuraOutputManagerListener*>(data);
    self->MockOnPhysicalSize(output, width, height);
  }
  MOCK_METHOD(void,
              MockOnPhysicalSize,
              (wl_output * output, int32_t width, int32_t height));

  static void OnInsets(void* data,
                       zaura_output_manager* output_manager,
                       wl_output* output,
                       int32_t top,
                       int32_t left,
                       int32_t bottom,
                       int32_t right) {
    auto* self = static_cast<MockAuraOutputManagerListener*>(data);
    self->MockOnInsets(output, top, left, bottom, right);
  }
  MOCK_METHOD(void,
              MockOnInsets,
              (wl_output * output,
               int32_t top,
               int32_t left,
               int32_t bottom,
               int32_t right));

  static void OnDeviceScaleFactor(void* data,
                                  zaura_output_manager* output_manager,
                                  wl_output* output,
                                  uint32_t scale_as_uint) {
    auto* self = static_cast<MockAuraOutputManagerListener*>(data);
    self->MockOnDeviceScaleFactor(output, scale_as_uint);
  }
  MOCK_METHOD(void,
              MockOnDeviceScaleFactor,
              (wl_output * output, uint32_t scale_as_uint));

  static void OnLogicalTransform(void* data,
                                 zaura_output_manager* output_manager,
                                 wl_output* output,
                                 int32_t transform) {
    auto* self = static_cast<MockAuraOutputManagerListener*>(data);
    self->MockOnLogicalTransform(output, transform);
  }
  MOCK_METHOD(void,
              MockOnLogicalTransform,
              (wl_output * output, int32_t transform));

  static void OnPanelTransform(void* data,
                               zaura_output_manager* output_manager,
                               wl_output* output,
                               int32_t transform) {
    auto* self = static_cast<MockAuraOutputManagerListener*>(data);
    self->MockOnPanelTransform(output, transform);
  }
  MOCK_METHOD(void,
              MockOnPanelTransform,
              (wl_output * output, int32_t transform));

  static void OnName(void* data,
                     zaura_output_manager* output_manager,
                     wl_output* output,
                     const char* name) {
    auto* self = static_cast<MockAuraOutputManagerListener*>(data);
    self->MockOnName(output, name);
  }
  MOCK_METHOD(void, MockOnName, (wl_output * output, const char* name));

  static void OnDescription(void* data,
                            zaura_output_manager* output_manager,
                            wl_output* output,
                            const char* description) {
    auto* self = static_cast<MockAuraOutputManagerListener*>(data);
    self->MockOnDescription(output, description);
  }
  MOCK_METHOD(void,
              MockOnDescription,
              (wl_output * output, const char* description));

  static void OnActivated(void* data,
                          zaura_output_manager* output_manager,
                          wl_output* output) {
    auto* self = static_cast<MockAuraOutputManagerListener*>(data);
    self->MockOnActivated(output);
  }
  MOCK_METHOD(void, MockOnActivated, (wl_output * output));

  static void OnOverscanInsets(void* data,
                               zaura_output_manager* output_manager,
                               wl_output* output,
                               int32_t top,
                               int32_t left,
                               int32_t bottom,
                               int32_t right) {
    auto* self = static_cast<MockAuraOutputManagerListener*>(data);
    self->MockOnOverscanInsets(output, top, left, bottom, right);
  }
  MOCK_METHOD(void,
              MockOnOverscanInsets,
              (wl_output * output,
               int32_t top,
               int32_t left,
               int32_t bottom,
               int32_t right));
};

class AuraOutputManagerTest : public test::WaylandServerTest {
 public:
  AuraOutputManagerTest() = default;
  AuraOutputManagerTest(const AuraOutputManagerTest&) = delete;
  AuraOutputManagerTest& operator=(const AuraOutputManagerTest&) = delete;
  ~AuraOutputManagerTest() override = default;

  std::unique_ptr<test::TestClient> InitOnClientThread() override {
    mock_aura_output_manager_ =
        std::make_unique<::testing::NiceMock<MockAuraOutputManagerListener>>();

    auto test_client = test::WaylandServerTest::InitOnClientThread();

    static constexpr zaura_output_manager_listener
        zaura_output_manager_listener = {
            &MockAuraOutputManagerListener::OnDone,
            &MockAuraOutputManagerListener::OnDisplayId,
            &MockAuraOutputManagerListener::OnLogicalPosition,
            &MockAuraOutputManagerListener::OnLogicalSize,
            &MockAuraOutputManagerListener::OnPhysicalSize,
            &MockAuraOutputManagerListener::OnInsets,
            &MockAuraOutputManagerListener::OnDeviceScaleFactor,
            &MockAuraOutputManagerListener::OnLogicalTransform,
            &MockAuraOutputManagerListener::OnPanelTransform,
            &MockAuraOutputManagerListener::OnName,
            &MockAuraOutputManagerListener::OnDescription,
            &MockAuraOutputManagerListener::OnActivated,
            &MockAuraOutputManagerListener::OnOverscanInsets};
    zaura_output_manager_add_listener(test_client->aura_output_manager(),
                                      &zaura_output_manager_listener,
                                      mock_aura_output_manager_.get());

    return test_client;
  }

  // Creates a wl_client instance for this test.
  void CreateClient() {
    ASSERT_FALSE(client_);
    socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds_);
    client_ = wl_client_create(server_->GetWaylandDisplay(), fds_[0]);
  }

  // Destroys the wl_client instance for this test if it exists.
  void DestroyClient() {
    if (client_) {
      wl_client_destroy(client_.ExtractAsDangling());
      close(fds_[1]);
      client_ = nullptr;
    }
  }

 protected:
  std::unique_ptr<MockAuraOutputManagerListener> mock_aura_output_manager_;
  int fds_[2] = {0, 0};
  raw_ptr<wl_client> client_ = nullptr;
};

}  // namespace

// Regression test for crbug.com/1433187. Ensures AuraOutputManager::Get() does
// not cause UAF crashes by attempting to iterate over resources belonging to a
// client that has started destruction.
TEST_F(AuraOutputManagerTest, GetterReturnsNullAfterClientDestroyed) {
  CreateClient();
  EXPECT_FALSE(IsClientDestroyed(client_));

  // Create a resource associated with the client.
  wl_resource* output_resource =
      wl_resource_create(client_, &wl_output_interface, 2, 0);
  wl_resource_set_user_data(output_resource, client_);

  // Ensure that calls to AuraOutputManager::Get() after client destruction
  // does not result in UAF crashes. This callback will run after the client's
  // destruction sequence begins and associated resources are freed.
  auto wl_resource_callback = [](wl_resource* resource) {
    wl_client* client =
        static_cast<wl_client*>(wl_resource_get_user_data(resource));
    EXPECT_TRUE(IsClientDestroyed(client));
    EXPECT_FALSE(AuraOutputManager::Get(client));
  };
  wl_resource_set_destructor(output_resource, wl_resource_callback);

  DestroyClient();
}

TEST_F(AuraOutputManagerTest, SendOverscanInsets) {
  wl_output* client_output = nullptr;

  {
    // Start with no overscan.
    UpdateDisplay("800x600");
    PostToClientAndWait(
        [&](test::TestClient* client) { client_output = client->output(); });

    ::testing::InSequence seq;
    const gfx::Insets no_overscan =
        display_manager()
            ->GetDisplayInfo(GetPrimaryDisplay().id())
            .GetOverscanInsetsInPixel();
    EXPECT_TRUE(no_overscan.IsEmpty());

    EXPECT_CALL(*mock_aura_output_manager_,
                MockOnOverscanInsets(client_output, no_overscan.top(),
                                     no_overscan.left(), no_overscan.bottom(),
                                     no_overscan.right()));
    EXPECT_CALL(*mock_aura_output_manager_, MockOnDone(client_output));
    display_manager()->NotifyDidProcessDisplayChanges(
        {/*added_displays=*/{},
         /*removed_displays=*/{},
         /*display_metrics_changes=*/{{GetPrimaryDisplay(), 0xFFFFFFFF}}});
    PostToClientAndWait([] {});
    ::testing::Mock::VerifyAndClearExpectations(
        mock_aura_output_manager_.get());
  }

  {
    // With zoom to verify dip to physical pixel conversion.
    UpdateDisplay("800x600*2.0");
    PostToClientAndWait([] {});

    ::testing::InSequence seq;
    const gfx::Insets overscan_in_dip = gfx::Insets::TLBR(5, 2, 10, 7);
    const gfx::Insets overscan_in_pixel =
        gfx::ScaleToFlooredInsets(overscan_in_dip, 2.0);
    EXPECT_CALL(*mock_aura_output_manager_,
                MockOnOverscanInsets(client_output, overscan_in_pixel.top(),
                                     overscan_in_pixel.left(),
                                     overscan_in_pixel.bottom(),
                                     overscan_in_pixel.right()));
    EXPECT_CALL(*mock_aura_output_manager_, MockOnDone(client_output));
    display_manager()->SetOverscanInsets(GetPrimaryDisplay().id(),
                                         overscan_in_dip);
    PostToClientAndWait([] {});
    ::testing::Mock::VerifyAndClearExpectations(
        mock_aura_output_manager_.get());
  }

  {
    // With rotation.
    UpdateDisplay("800x600*2.0/r");
    PostToClientAndWait([] {});

    ::testing::InSequence seq;
    const gfx::Insets overscan_in_dip = gfx::Insets::TLBR(7, 9, 5, 4);
    const gfx::Insets overscan_in_pixel =
        gfx::ScaleToFlooredInsets(overscan_in_dip, 2.0);
    EXPECT_CALL(*mock_aura_output_manager_,
                MockOnOverscanInsets(client_output, overscan_in_pixel.top(),
                                     overscan_in_pixel.left(),
                                     overscan_in_pixel.bottom(),
                                     overscan_in_pixel.right()));
    EXPECT_CALL(*mock_aura_output_manager_, MockOnDone(client_output));
    display_manager()->SetOverscanInsets(GetPrimaryDisplay().id(),
                                         overscan_in_dip);
    PostToClientAndWait([] {});
    ::testing::Mock::VerifyAndClearExpectations(
        mock_aura_output_manager_.get());
  }
}

}  // namespace exo::wayland
