// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_PRIVATE_NETWORK_SETTINGS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_PRIVATE_NETWORK_SETTINGS_H_

class HostContentSettingsMap;

namespace url {
class Origin;
}  // namespace url

namespace content_settings {

// Returns whether |origin| should be allowed to make insecure private network
// requests, given the settings contained in |map|.
//
// |map| must not be nullptr. Caller retains ownership.
// |origin| should identify the frame initiating a request.
bool ShouldAllowInsecurePrivateNetworkRequests(
    const HostContentSettingsMap* map,
    const url::Origin& origin);

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_PRIVATE_NETWORK_SETTINGS_H_
