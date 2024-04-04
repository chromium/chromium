// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zaura_output_manager_v2.h"

#include "base/bit_cast.h"
#include "components/exo/wayland/output_controller_test_api.h"
#include "components/exo/wayland/output_metrics.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/test/test_client.h"
#include "components/exo/wayland/test/wayland_server_test.h"
#include "components/exo/wayland/wayland_display_output.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo::wayland {

namespace {

using ::testing::_;
using ::testing::ExpectationSet;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::StrEq;

class MockGlobalsObserver : public clients::Globals::TestObserver {
 public:
  MOCK_METHOD(void,
              OnRegistryGlobal,
              (uint32_t id, const char* interface, uint32_t version),
              (override));
  MOCK_METHOD(void, OnRegistryGlobalRemove, (uint32_t id), (override));
};

class MockAuraOutputManagerListener {
 public:
  static void OnDone(void* data, zaura_output_manager_v2* output_manager) {
    static_cast<MockAuraOutputManagerListener*>(data)->MockOnDone();
  }
  static void OnDisplayId(void* data,
                          zaura_output_manager_v2* output_manager,
                          uint32_t output_name,
                          uint32_t display_id_hi,
                          uint32_t display_id_lo) {
    static_cast<MockAuraOutputManagerListener*>(data)->MockOnDisplayId(
        output_name, display_id_hi, display_id_lo);
  }
  static void OnLogicalPosition(void* data,
                                zaura_output_manager_v2* output_manager,
                                uint32_t output_name,
                                int32_t x,
                                int32_t y) {
    static_cast<MockAuraOutputManagerListener*>(data)->MockOnLogicalPosition(
        output_name, x, y);
  }
  static void OnLogicalSize(void* data,
                            zaura_output_manager_v2* output_manager,
                            uint32_t output_name,
                            int32_t width,
                            int32_t height) {
    static_cast<MockAuraOutputManagerListener*>(data)->MockOnLogicalSize(
        output_name, width, height);
  }
  static void OnPhysicalSize(void* data,
                             zaura_output_manager_v2* output_manager,
                             uint32_t output_name,
                             int32_t width,
                             int32_t height) {
    static_cast<MockAuraOutputManagerListener*>(data)->MockOnPhysicalSize(
        output_name, width, height);
  }
  static void OnInsets(void* data,
                       zaura_output_manager_v2* output_manager,
                       uint32_t output_name,
                       int32_t top,
                       int32_t left,
                       int32_t bottom,
                       int32_t right) {
    static_cast<MockAuraOutputManagerListener*>(data)->MockOnInsets(
        output_name, top, left, bottom, right);
  }
  static void OnDeviceScaleFactor(void* data,
                                  zaura_output_manager_v2* output_manager,
                                  uint32_t output_name,
                                  uint32_t scale_as_uint) {
    static_cast<MockAuraOutputManagerListener*>(data)->MockOnDeviceScaleFactor(
        output_name, scale_as_uint);
  }
  static void OnLogicalTransform(void* data,
                                 zaura_output_manager_v2* output_manager,
                                 uint32_t output_name,
                                 int32_t transform) {
    static_cast<MockAuraOutputManagerListener*>(data)->MockOnLogicalTransform(
        output_name, transform);
  }
  static void OnPanelTransform(void* data,
                               zaura_output_manager_v2* output_manager,
                               uint32_t output_name,
                               int32_t transform) {
    static_cast<MockAuraOutputManagerListener*>(data)->MockOnPanelTransform(
        output_name, transform);
  }
  static void OnName(void* data,
                     zaura_output_manager_v2* output_manager,
                     uint32_t output_name,
                     const char* name) {
    static_cast<MockAuraOutputManagerListener*>(data)->MockOnName(output_name,
                                                                  name);
  }
  static void OnDescription(void* data,
                            zaura_output_manager_v2* output_manager,
                            uint32_t output_name,
                            const char* description) {
    static_cast<MockAuraOutputManagerListener*>(data)->MockOnDescription(
        output_name, description);
  }
  static void OnOverscanInsets(void* data,
                               zaura_output_manager_v2* output_manager,
                               uint32_t output_name,
                               int32_t top,
                               int32_t left,
                               int32_t bottom,
                               int32_t right) {
    static_cast<MockAuraOutputManagerListener*>(data)->MockOnOverscanInsets(
        output_name, top, left, bottom, right);
  }
  static void OnActivated(void* data,
                          zaura_output_manager_v2* output_manager,
                          uint32_t output_name) {
    static_cast<MockAuraOutputManagerListener*>(data)->MockOnActivated(
        output_name);
  }

  MOCK_METHOD(void, MockOnDone, ());
  MOCK_METHOD(void,
              MockOnDisplayId,
              (uint32_t output_name,
               uint32_t display_id_hi,
               uint32_t display_id_lo));
  MOCK_METHOD(void,
              MockOnLogicalPosition,
              (uint32_t output_name, int32_t x, int32_t y));
  MOCK_METHOD(void,
              MockOnLogicalSize,
              (uint32_t output_name, int32_t width, int32_t height));
  MOCK_METHOD(void,
              MockOnPhysicalSize,
              (uint32_t output_name, int32_t width, int32_t height));
  MOCK_METHOD(void,
              MockOnInsets,
              (uint32_t output_name,
               int32_t top,
               int32_t left,
               int32_t bottom,
               int32_t right));
  MOCK_METHOD(void,
              MockOnDeviceScaleFactor,
              (uint32_t output_name, uint32_t scale_as_uint));
  MOCK_METHOD(void,
              MockOnLogicalTransform,
              (uint32_t output_name, int32_t transform));
  MOCK_METHOD(void,
              MockOnPanelTransform,
              (uint32_t output_name, int32_t transform));
  MOCK_METHOD(void, MockOnName, (uint32_t output_name, const char* name));
  MOCK_METHOD(void,
              MockOnDescription,
              (uint32_t output_name, const char* description));
  MOCK_METHOD(void,
              MockOnOverscanInsets,
              (uint32_t output_name,
               int32_t top,
               int32_t left,
               int32_t bottom,
               int32_t right));
  MOCK_METHOD(void, MockOnActivated, (uint32_t output_name));
};

// Server test asserting clients receive the events specified by the
// aura_output_manager_v2 interface in the order expected.
class AuraOutputManagerV2Test : public test::WaylandServerTest {
 public:
  // test::WaylandServerTest:
  std::unique_ptr<test::TestClient> InitOnClientThread() override {
    auto test_client = test::WaylandServerTest::InitOnClientThread();

    test_client->globals().set_observer_for_testing(&mock_globals_observer_);

    static constexpr zaura_output_manager_v2_listener
        zaura_output_manager_v2_listener = {
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
            &MockAuraOutputManagerListener::OnOverscanInsets,
            &MockAuraOutputManagerListener::OnActivated};
    zaura_output_manager_v2_add_listener(test_client->aura_output_manager_v2(),
                                         &zaura_output_manager_v2_listener,
                                         &mock_aura_output_manager_);

    return test_client;
  }

 protected:
  void ExpectMetrics(uint32_t output_name,
                     const OutputMetrics& metrics,
                     ExpectationSet& expectations) {
    expectations +=
        EXPECT_CALL(mock_aura_output_manager_,
                    MockOnDisplayId(output_name, metrics.display_id.high,
                                    metrics.display_id.low));
    expectations += EXPECT_CALL(
        mock_aura_output_manager_,
        MockOnLogicalPosition(output_name, metrics.logical_origin.x(),
                              metrics.logical_origin.y()));
    expectations +=
        EXPECT_CALL(mock_aura_output_manager_,
                    MockOnLogicalSize(output_name, metrics.logical_size.width(),
                                      metrics.logical_size.height()));
    expectations += EXPECT_CALL(
        mock_aura_output_manager_,
        MockOnPhysicalSize(output_name, metrics.physical_size_px.width(),
                           metrics.physical_size_px.height()));
    expectations +=
        EXPECT_CALL(mock_aura_output_manager_,
                    MockOnInsets(output_name, metrics.logical_insets.top(),
                                 metrics.logical_insets.left(),
                                 metrics.logical_insets.bottom(),
                                 metrics.logical_insets.right()));
    expectations += EXPECT_CALL(
        mock_aura_output_manager_,
        MockOnDeviceScaleFactor(output_name, base::bit_cast<uint32_t>(
                                                 metrics.device_scale_factor)));
    expectations += EXPECT_CALL(
        mock_aura_output_manager_,
        MockOnLogicalTransform(output_name, metrics.logical_transform));
    expectations +=
        EXPECT_CALL(mock_aura_output_manager_,
                    MockOnPanelTransform(output_name, metrics.panel_transform));
    expectations +=
        EXPECT_CALL(mock_aura_output_manager_,
                    MockOnOverscanInsets(
                        output_name, metrics.physical_overscan_insets.top(),
                        metrics.physical_overscan_insets.left(),
                        metrics.physical_overscan_insets.bottom(),
                        metrics.physical_overscan_insets.right()));
  }

  NiceMock<MockAuraOutputManagerListener> mock_aura_output_manager_;
  NiceMock<MockGlobalsObserver> mock_globals_observer_;
};

}  // namespace

TEST_F(AuraOutputManagerV2Test, ActiveOutputMetricsUpdate) {
  // Start with a single display and round-trip with client to clear the event
  // queue.
  UpdateDisplay("800x600");
  PostToClientAndWait([] {});

  const auto* screen = display::Screen::GetScreen();
  ASSERT_EQ(1u, screen->GetAllDisplays().size());

  const int64_t primary_id = screen->GetAllDisplays()[0].id();
  auto* output_controller = server_->output_controller_for_testing();
  OutputControllerTestApi output_controller_test_api(*output_controller);
  const WaylandDisplayOutput* primary_output =
      output_controller_test_api.GetWaylandDisplayOutput(primary_id);
  const uint32_t primary_output_name =
      wl_global_get_name(primary_output->global(), client_resource_.get());

  // Update the display, expect to see updated metrics only followed by done.
  OutputMetrics metrics = primary_output->metrics();
  metrics.physical_size_px.SetSize(1200, 800);
  metrics.logical_size.SetSize(1200, 800);

  ExpectationSet expected_events;
  expected_events += EXPECT_CALL(mock_globals_observer_,
                                 OnRegistryGlobal(_, StrEq("wl_output"), _))
                         .Times(0);
  expected_events +=
      EXPECT_CALL(mock_globals_observer_, OnRegistryGlobalRemove(_)).Times(0);
  ExpectMetrics(primary_output_name, metrics, expected_events);
  EXPECT_CALL(mock_aura_output_manager_, MockOnDone()).After(expected_events);

  UpdateDisplay("1200x800");
  PostToClientAndWait([] {});

  Mock::VerifyAndClearExpectations(&mock_aura_output_manager_);
  Mock::VerifyAndClearExpectations(&mock_globals_observer_);

  // Subsequent updates should send new updates as expected.
  metrics = primary_output->metrics();
  metrics.physical_size_px.SetSize(1600, 1200);
  metrics.logical_size.SetSize(1600, 1200);

  ExpectationSet new_expected_events;
  new_expected_events += EXPECT_CALL(mock_globals_observer_,
                                     OnRegistryGlobal(_, StrEq("wl_output"), _))
                             .Times(0);
  new_expected_events +=
      EXPECT_CALL(mock_globals_observer_, OnRegistryGlobalRemove(_)).Times(0);
  ExpectMetrics(primary_output_name, metrics, new_expected_events);
  EXPECT_CALL(mock_aura_output_manager_, MockOnDone())
      .After(new_expected_events);

  UpdateDisplay("1600x1200");
  PostToClientAndWait([] {});
}

TEST_F(AuraOutputManagerV2Test, ActiveOutputsAdded) {
  // Start with a single display and round-trip with client to clear the event
  // queue.
  UpdateDisplay("800x600");
  const auto* screen = display::Screen::GetScreen();
  ASSERT_EQ(1u, screen->GetAllDisplays().size());
  PostToClientAndWait([](test::TestClient* client) {
    ASSERT_EQ(1u, client->globals().outputs.size());
  });

  // Add two new displays to the configuration, events for two new outputs and
  // their corresponding metrics should be propagated to clients.
  ExpectationSet expected_events;
  expected_events += EXPECT_CALL(mock_globals_observer_,
                                 OnRegistryGlobal(_, StrEq("wl_output"), _))
                         .Times(2);
  expected_events +=
      EXPECT_CALL(mock_globals_observer_, OnRegistryGlobalRemove(_)).Times(0);
  expected_events +=
      EXPECT_CALL(mock_aura_output_manager_, MockOnLogicalSize(_, 1200, 800));
  expected_events +=
      EXPECT_CALL(mock_aura_output_manager_, MockOnLogicalSize(_, 1600, 1200));
  EXPECT_CALL(mock_aura_output_manager_, MockOnDone()).After(expected_events);

  UpdateDisplay("800x600,1200x800,1600x1200");
  ASSERT_EQ(3u, screen->GetAllDisplays().size());
  PostToClientAndWait([](test::TestClient* client) {
    ASSERT_EQ(3u, client->globals().outputs.size());
  });
}

TEST_F(AuraOutputManagerV2Test, ActiveOutputsRemoved) {
  // Start multiple displays and round-trip with client to clear the event
  // queue.
  UpdateDisplay("800x600,1200x800,1600x1200");
  const auto* screen = display::Screen::GetScreen();
  ASSERT_EQ(3u, screen->GetAllDisplays().size());
  PostToClientAndWait([](test::TestClient* client) {
    ASSERT_EQ(3u, client->globals().outputs.size());
  });

  const int64_t secondary_id = screen->GetAllDisplays()[1].id();
  const int64_t tertiary_id = screen->GetAllDisplays()[2].id();

  auto* output_controller = server_->output_controller_for_testing();
  OutputControllerTestApi output_controller_test_api(*output_controller);
  const uint64_t secondary_output_name = wl_global_get_name(
      output_controller_test_api.GetWaylandDisplayOutput(secondary_id)
          ->global(),
      client_resource_.get());
  const uint64_t tertiary_output_name = wl_global_get_name(
      output_controller_test_api.GetWaylandDisplayOutput(tertiary_id)->global(),
      client_resource_.get());

  // Remove two displays from the configuration, events for the two removals
  // should be propagated to clients.
  ExpectationSet expected_events;
  expected_events += EXPECT_CALL(mock_globals_observer_,
                                 OnRegistryGlobal(_, StrEq("wl_output"), _))
                         .Times(0);
  expected_events += EXPECT_CALL(mock_globals_observer_,
                                 OnRegistryGlobalRemove(secondary_output_name));
  expected_events += EXPECT_CALL(mock_globals_observer_,
                                 OnRegistryGlobalRemove(tertiary_output_name));
  expected_events +=
      EXPECT_CALL(mock_aura_output_manager_, MockOnLogicalSize(_, _, _))
          .Times(0);
  EXPECT_CALL(mock_aura_output_manager_, MockOnDone()).After(expected_events);

  UpdateDisplay("800x600");
  ASSERT_EQ(1u, screen->GetAllDisplays().size());
  // TODO(tluk): Update Globals to correctly handle wl_registry.global_remove
  // events.
  PostToClientAndWait([] {});
}

TEST_F(AuraOutputManagerV2Test, ActivateDisplay) {
  // Start with a two displays and round-trip with client to clear the event
  // queue.
  auto* output_controller = server_->output_controller_for_testing();
  OutputControllerTestApi output_controller_test_api(*output_controller);
  UpdateDisplay("800x600,800x600");
  auto* screen = display::Screen::GetScreen();
  ASSERT_EQ(2u, screen->GetAllDisplays().size());
  PostToClientAndWait([](test::TestClient* client) {
    ASSERT_EQ(2u, client->globals().outputs.size());
  });

  const int64_t primary_id = screen->GetAllDisplays()[0].id();
  const uint64_t primary_output_name = wl_global_get_name(
      output_controller_test_api.GetWaylandDisplayOutput(primary_id)->global(),
      client_resource_.get());
  const int64_t secondary_id = screen->GetAllDisplays()[1].id();
  const uint64_t secondary_output_name = wl_global_get_name(
      output_controller_test_api.GetWaylandDisplayOutput(secondary_id)
          ->global(),
      client_resource_.get());

  // Force activation on the secondary display.
  EXPECT_NE(secondary_id,
            output_controller_test_api.GetDispatchedActivatedDisplayId());
  screen->SetDisplayForNewWindows(secondary_id);
  EXPECT_EQ(secondary_id,
            output_controller_test_api.GetDispatchedActivatedDisplayId());
  EXPECT_CALL(mock_aura_output_manager_,
              MockOnActivated(secondary_output_name));
  PostToClientAndWait([] {});

  // Force activation back to the primary display.
  screen->SetDisplayForNewWindows(primary_id);
  EXPECT_EQ(primary_id,
            output_controller_test_api.GetDispatchedActivatedDisplayId());
  EXPECT_CALL(mock_aura_output_manager_, MockOnActivated(primary_output_name));
  PostToClientAndWait([] {});
}

}  // namespace exo::wayland
