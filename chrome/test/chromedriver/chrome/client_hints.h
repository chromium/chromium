// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CLIENT_HINTS_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CLIENT_HINTS_H_

#include <optional>
#include <string>
#include <vector>

struct BrandVersion {
  // Browser brand.
  // Examples: Chrome, Chromium.
  // Can be any value.
  std::string brand;
  // Browser version.
  // The value is:
  // * "major version" for "brands",
  // * "full version" for "fullVersionList".
  std::string version;
};

// User-Agent Client Hints
// as specified by https://wicg.github.io/ua-client-hints/.
// S/A go/chromedriver-ua-ch-capabilities.
// S/A go/chromedriver-mobile-emulation-inferences.
struct ClientHints {
  ClientHints();
  ClientHints(const ClientHints&);
  ~ClientHints();
  ClientHints& operator=(const ClientHints&);
  // Optional high entropy "platform architecture" client hint.
  // Maps to Sec-CH-UA-Arch header value.
  // Available via navigator.userAgentData.getHighEntropyData() JS call.
  // Normally the value is "x86", "arm" or empty string.
  // The draft also explicitly permits any other fictitious value.
  std::string architecture;
  // Optional high entropy "platform bitness" client hint.
  // Maps to Sec-CH-UA-Bitness header value.
  // Available via navigator.userAgentData.getHighEntropyData() JS call.
  // Normally the value is "32" or "64".
  // The draft also explicitly permits any other fictitious value.
  std::string bitness;
  // Mandatory (for browser) branding client hint.
  // Maps to Sec-CH-UA header value.
  // Maps to navigator.userAgentData.brands JS value.
  // Servier and client code can figure out the browser brand and version using
  // this hint.
  // We prefer defaulting to the browser provided value.
  std::optional<std::vector<BrandVersion>> brands;
  // Optional high entropy "full version" client hint.
  // Maps to Sec-CH-UA-Full-Version-List header value
  // Available via navigator.userAgentData.getHighEntropyData() JS call.
  // We prefer defaulting to the browser provided value.
  std::optional<std::vector<BrandVersion>> full_version_list;
  // Mandatory "platform" client hint.
  // Maps to Sec-CH-UA-Platform header value.
  // Maps to navigator.userAgentData.platform JS value.
  // Normally the values are "Android", "Chrome OS", "Fuchsia", "iOS", "Linux",
  // "macOS", "Windows", or "Unknown".
  // The value can be any string though.
  // We have special treatment for some values returned by the function
  // GetPlatformForUAMetadata in components/embedder_support/user_agent_utils.cc
  std::string platform;
  // Optional high entropy "platform version" client hint.
  // Maps to Sec-CH-UA-Platform-Version header value.
  // Available via navigator.userAgentData.getHighEntropyData() JS call.
  // The value can be any string.
  // Examples: "NT 6.0", "15", "17G", etc.
  std::string platform_version;
  // Optional high entropy "model" client hint.
  // Maps to Sec-CH-UA-Model header value.
  // Available via navigator.userAgentData.getHighEntropyData() JS call.
  // The value can be any string.
  std::string model;
  // Optional high entropy "wow64-ness" client hint.
  // Maps to Sec-CH-UA-WoW64 header value.
  // Available via navigator.userAgentData.getHighEntropyData() JS call.
  // WoW64 stands for Windows 32 on Windows 64.
  // S/A: https://en.wikipedia.org/wiki/WoW64.
  bool wow64 = false;
  // Mandatory "mobileness" client hint.
  // Maps to Sec-CH-UA-Mobile header value.
  // Maps to navigator.userAgentData.mobile JS value.
  // Server or client code can figure out that the page is running in the mobile
  // browser by querying this hint.
  bool mobile = false;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CLIENT_HINTS_H_
