// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Hot Tip! Generate a tsconfig.json file to get language server support. Run:
// ash/webui/personalization_app/tools/gen_tsconfig.py --root_out_dir out/pc \
//   --gn_target chrome/test/data/webui/glic:build_ts

import type {GlicBrowserHost, GlicWebClient, PanelState} from '/glic/glic_api/glic_api.js';

import {createGlicHostRegistryOnLoad} from './api_boot.js';

// A dummy web client.
class WebClient implements GlicWebClient {
  host?: GlicBrowserHost;

  async initialize(glicBrowserHost: GlicBrowserHost): Promise<void> {
    this.host = glicBrowserHost;
  }

  async notifyPanelWillOpen(_panelState: PanelState): Promise<void> {}
}

async function main() {
  const registry = await createGlicHostRegistryOnLoad();
  registry.registerWebClient(new WebClient());
}

main();
