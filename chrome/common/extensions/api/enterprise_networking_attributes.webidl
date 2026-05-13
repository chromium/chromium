// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary NetworkDetails {
  // The device's MAC address.
  required DOMString macAddress;

  // The device's local IPv4 address (undefined if not configured).
  DOMString ipv4;

  // The device's local IPv6 address (undefined if not configured).
  DOMString ipv6;
};

// Use the <code>chrome.enterprise.networkingAttributes</code> API to read
// information about your current network.
// Note: This API is only available to extensions force-installed by enterprise
// policy.
[platforms = ("chromeos"),
 implemented_in = "chrome/browser/extensions/api/enterprise_networking_attributes/enterprise_networking_attributes_api.h"]
interface NetworkingAttributes {
  // Retrieves the network details of the device's default network.
  // If the user is not affiliated or the device is not connected to a
  // network, $(ref:runtime.lastError) will be set with a failure reason.
  // |Returns|: Returns a Promise which resolves with the device's default
  // network's $(ref:NetworkDetails).
  // |PromiseValue|: networkAddresses
  static Promise<NetworkDetails> getNetworkDetails();
};

partial interface Enterprise {
  static attribute NetworkingAttributes networkingAttributes;
};

partial interface Browser {
  static attribute Enterprise enterprise;
};
