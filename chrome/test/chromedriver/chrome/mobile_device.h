// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_MOBILE_DEVICE_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_MOBILE_DEVICE_H_

#include <memory>
#include <optional>
#include <string>

#include "chrome/test/chromedriver/chrome/client_hints.h"
#include "chrome/test/chromedriver/chrome/device_metrics.h"

class Status;

struct MobileDevice {
  MobileDevice();
  MobileDevice(const MobileDevice&);
  MobileDevice(MobileDevice&&);
  MobileDevice& operator=(const MobileDevice&);
  MobileDevice& operator=(MobileDevice&&);
  ~MobileDevice();

  // Returns the reduced User-agent string for
  // https://github.com/WICG/ua-client-hints.
  Status GetReducedUserAgent(std::string major_version,
                             std::string* reduced_user_agent) const;

  // Find an emulation preset by a device name.
  static Status FindMobileDevice(std::string device_name,
                                 MobileDevice* mobile_device);
  // List of platform names for the platforms where Chrome supports reduced user
  // agent.
  static std::vector<std::string> GetReducedUserAgentPlatforms();
  // Names of devices that have presets.
  static Status GetKnownMobileDeviceNamesForTesting(
      std::vector<std::string>* device_names);
  // Guess the platform to which the user agent belongs.
  static bool GuessPlatform(const std::string& user_agent,
                            std::string* platform);

  // Specifies viewport size, window decorations, etc.
  std::optional<DeviceMetrics> device_metrics;
  // User agent value of the browser.
  // Maps to "user-agent" header.
  // Maps to "navigator.userAgent JS value.
  // Inferred from client_hints if empty for the platforms listed in
  // MobileDevice::GetReducedUserAgentPlatrforms function result.
  std::optional<std::string> user_agent;
  // Client hints used by the browser.
  // S/A: chrome/test/chromedriver/chrome/client_hints.h
  std::optional<ClientHints> client_hints;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_MOBILE_DEVICE_H_
