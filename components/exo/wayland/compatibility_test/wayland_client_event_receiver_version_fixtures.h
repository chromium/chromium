// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_COMPATIBILITY_TEST_WAYLAND_CLIENT_EVENT_RECEIVER_VERSION_FIXTURES_H_
#define COMPONENTS_EXO_WAYLAND_COMPATIBILITY_TEST_WAYLAND_CLIENT_EVENT_RECEIVER_VERSION_FIXTURES_H_

// ===========================================================================
//
// This file contains optional fixture overrides/customizations for each of
// the generated fixtures. Each generated fixture has functions that can be
// overridden for:
//
// 1) Determining if the test should be skipped. The generated code skips
//    tests for interface versions not supported by the server.
//
// 2) Creating target and dependent interfaces for each test. For some tests,
//    the generated default creation steps may work to get a valid target
//    interface.
//
// 3) "Nudge" functions called by the client test to help inject or otherwise
//    trigger event generation by the server. The server may otherwise not
//    send certain events without some other trigger, such as keys being
//    pressed to generate keyboard input events.
//
// Understanding what can and may need to be overridden means examining the
// generated code in:
//
//   out/$target/gen/components/exo/wayland/compatibility_test/
//       all_generated_client_event_receiver_version_tests.cc
//
// At present this file contains examples for only the first case. Getting more
// tests to work (see TODO's below) will require the other two types of
// overrides.
// ===========================================================================

namespace exo {
namespace wayland {
namespace compatibility {
namespace test {
namespace {

struct ZauraSurfaceEventTest : public ZauraSurfaceEventTestBase {
  using Base = ZauraSurfaceEventTestBase;
  bool ShouldSkip(uint32_t version) const noexcept override;
};

bool ZauraSurfaceEventTest::ShouldSkip(uint32_t version) const noexcept {
  // TODO(b/157254342): Remove the skip for versions >= 8
  if (version >= 8)
    return true;
  return Base::ShouldSkip(version);
}

struct WlDataOfferEventTest : public WlDataOfferEventTestBase {
  bool ShouldSkip(uint32_t) const noexcept override;
};

bool WlDataOfferEventTest::ShouldSkip(uint32_t) const noexcept {
  // TODO(b/157254342): Remove the skip for any version
  return true;
}

struct WlDataSourceEventTest : public WlDataSourceEventTestBase {
  bool ShouldSkip(uint32_t) const noexcept override;
};

bool WlDataSourceEventTest::ShouldSkip(uint32_t) const noexcept {
  // TODO(b/157254342): Remove the skip for any version
  return true;
}

struct WlPointerEventTest : public WlPointerEventTestBase {
  using Base = WlPointerEventTestBase;
  bool ShouldSkip(uint32_t) const noexcept override;
};

bool WlPointerEventTest::ShouldSkip(uint32_t version) const noexcept {
  // TODO(b/157254342): Remove the skip for versions >= 5
  if (version >= 5)
    return true;
  return Base::ShouldSkip(version);
}

struct WlTouchEventTest : public WlTouchEventTestBase {
  using Base = WlTouchEventTestBase;
  bool ShouldSkip(uint32_t) const noexcept override;
};

bool WlTouchEventTest::ShouldSkip(uint32_t version) const noexcept {
  // TODO(b/157254342): Remove the skip for versions >= 6
  if (version >= 6)
    return true;
  return Base::ShouldSkip(version);
}

struct XdgPopupEventTest : public XdgPopupEventTestBase {
  bool ShouldSkip(uint32_t) const noexcept override;
};

bool XdgPopupEventTest::ShouldSkip(uint32_t) const noexcept {
  // TODO(b/157254342): Remove the skip for any version
  return true;
}

struct ZcrGamepadV2EventTest : public ZcrGamepadV2EventTestBase {
  bool ShouldSkip(uint32_t) const noexcept override;
};

bool ZcrGamepadV2EventTest::ShouldSkip(uint32_t) const noexcept {
  // TODO(b/157254342): Remove the skip for any version
  return true;
}

struct ZcrKeyboardDeviceConfigurationV1EventTest
    : public ZcrKeyboardDeviceConfigurationV1EventTestBase {
  bool ShouldSkip(uint32_t) const noexcept override;
};

bool ZcrKeyboardDeviceConfigurationV1EventTest::ShouldSkip(
    uint32_t) const noexcept {
  // TODO(b/157254342): Remove the skip for any version
  return true;
}

struct ZcrRemoteShellV1EventTest : public ZcrRemoteShellV1EventTestBase {
  using Base = ZcrRemoteShellV1EventTestBase;
  bool ShouldSkip(uint32_t) const noexcept override;
};

bool ZcrRemoteShellV1EventTest::ShouldSkip(uint32_t version) const noexcept {
  // Note: versions < 20 are NO LONGER supported by the server.
  if (version < 20)
    return true;
  return Base::ShouldSkip(version);
}

struct ZcrRemoteSurfaceV1EventTest : public ZcrRemoteSurfaceV1EventTestBase {
  using Base = ZcrRemoteSurfaceV1EventTestBase;
  bool ShouldSkip(uint32_t) const noexcept override;
};

bool ZcrRemoteSurfaceV1EventTest::ShouldSkip(uint32_t version) const noexcept {
  // TODO(b/157254342): Remove the skip for any version
  return true;

  // Note: versions < 20 are NO LONGER supported by the server.
  // if (version < 20)
  //   return true;
  // return Base::ShouldSkip(version);
}

}  // namespace
}  // namespace test
}  // namespace compatibility
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_COMPATIBILITY_TEST_WAYLAND_CLIENT_EVENT_RECEIVER_VERSION_FIXTURES_H_
