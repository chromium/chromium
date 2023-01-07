// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/constants/dbus_switches.h"

namespace chromeos {
namespace switches {

// Used in AttestationClient to determine which Google Privacy CA to use for
// attestation.
const char kAttestationServer[] = "attestation-server";

// Enables BIOD fake behavior. If the switch is set, fake biod D-Bus client is
// initialized and BIOD events do not reach chrome.
const char kBiodFake[] = "biod-fake";

// Enables cros disks fake behavior. If the switch is set, fake cros disk D-Bus
// client is initialized and USB events do not reach chrome.
const char kCrosDisksFake[] = "cros-disks-fake";

// Forces the stub implementation of D-Bus clients.
// Using stub D-Bus clients is the default for non-Chrome OS environment, to use
// real D-Bus clients in non-Chrome OS environment, set this flag to "never".
const char kDbusStub[] = "dbus-stub";

// Path to a OOBE configuration JSON file (used by FakeOobeConfigurationClient).
const char kFakeOobeConfiguration[] = "fake-oobe-configuration-file";

// Overrides Shill stub behavior. By default, ethernet, wifi and vpn are
// enabled, and transitions occur instantaneously. Multiple options can be
// comma separated (no spaces). Note: all options are in the format 'foo=x'.
// Values are case sensitive and based on Shill names in service_constants.h.
// See FakeShillManagerClient::SetInitialNetworkState for implementation.
// Examples:
//  'clear=1' - Clears all default configurations
//  'wifi=on' - A wifi network is initially connected ('1' also works)
//  'wifi=off' - Wifi networks are all initially disconnected ('0' also works)
//  'wifi=disabled' - Wifi is initially disabled
//  'wifi=none' - Wifi is unavailable
//  'wifi=portal' - Wifi connection will be in Portal state
//  'cellular=1' - Cellular is initially connected
//  'cellular=LTE' - Cellular is initially connected, technology is LTE
//  'interactive=3' - Interactive mode, connect/scan/etc requests take 3 secs
const char kShillStub[] = "shill-stub";

// Enables Hermes fake behavior. By default, no carrier profiles are setup.
// If a value of "on" is passed for this switch, then hermes fakes are
// initialized with a single installed carrier profile. Fake cellular service
// corresponding to carrier profiles are also setup in Shill.
const char kHermesFake[] = "hermes-fake";

// Sends test messages on first call to RequestUpdate (stub only).
const char kSmsTestMessages[] = "sms-test-messages";

// Used by FakeDebugDaemonClient to specify that the system is running in dev
// mode when running in a Linux environment. The dev mode probing is done by
// session manager.
const char kSystemDevMode[] = "system-developer-mode";

// Makes Chrome register the maximum dark suspend delay possible on Chrome OS
// i.e. give the device the maximum amount of time to do its work in dark
// resume as is allowed by the power manager.
const char kRegisterMaxDarkSuspendDelay[] = "register-max-dark-suspend-delay";

}  // namespace switches
}  // namespace chromeos
