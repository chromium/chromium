// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {GlicBrowserHost, GlicWebClient, OpenPanelInfo, PanelOpeningData, PanelState} from '/glic/glic_api/glic_api.js';
import {WebClientMode} from '/glic/glic_api/glic_api.js';

import {createGlicHostRegistryOnLoad} from '../api_boot.js';

performance.mark('glic-client-main-script-start');

class WebClient implements GlicWebClient {
  async initialize(_browser: GlicBrowserHost): Promise<void> {
    performance.mark('glic-client-initialized');
  }

  async notifyPanelWillOpen(_panelOpeningData: PanelOpeningData&PanelState):
      Promise<OpenPanelInfo> {
    performance.mark('glic-client-panel-opened');
    return {startingMode: WebClientMode.TEXT};
  }
}

const client = new WebClient();

createGlicHostRegistryOnLoad().then((registry) => {
  registry.registerWebClient(client);
});
