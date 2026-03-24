// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_TAB_NETWORK_STATE_H_
#define COMPONENTS_TABS_PUBLIC_TAB_NETWORK_STATE_H_

namespace content {
class WebContents;
}

namespace tabs {

// The types of network activity for a tab. The network state of a tab may be
// used to alter the UI (e.g. show different kinds of loading animations).
enum class TabNetworkState {
  kNone,     // No network activity.
  kWaiting,  // Waiting for a connection.
  kLoading,  // Connected, transferring data.
  kError,    // Encountered a network error.
};

// Computes the TabNetworkState for the given WebContents.
TabNetworkState TabNetworkStateForWebContents(content::WebContents* contents);

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_TAB_NETWORK_STATE_H_
