// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {GlicBrowserHost, GlicWebClient, OpenPanelInfo, PanelOpeningData, PanelState, WebClientMode} from '/glic/glic_api/glic_api.js';

import {createGlicHostRegistryOnLoad} from '../api_boot.js';

performance.mark('glic-client-main-script-start');

// Type-only imports above keep glic_api.js / glic_api_generated.js /
// geic_api.js out of the runtime module graph. The production client bundles
// its own copy of the API types and never fetches these files, so loading
// them here would charge the benchmark for harness-only work.
const kWebClientModeText = 0 as WebClientMode;

class WebClient implements GlicWebClient {
  async initialize(_browser: GlicBrowserHost): Promise<void> {
    performance.mark('glic-client-initialized');
  }

  async notifyPanelWillOpen(_panelOpeningData: PanelOpeningData&PanelState):
      Promise<OpenPanelInfo> {
    performance.mark('glic-client-panel-opened');
    return {startingMode: kWebClientModeText};
  }
}

const client = new WebClient();

createGlicHostRegistryOnLoad().then((registry) => {
  registry.registerWebClient(client);
});
