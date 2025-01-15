// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Hot Tip! Generate a tsconfig.json file to get language server support. Run:
// ash/webui/personalization_app/tools/gen_tsconfig.py --root_out_dir out/pc \
//   --gn_target chrome/test/data/webui/glic:build_ts

import type {GlicApiBootMessage, GlicBrowserHost, GlicHostRegistry, GlicWebClient, PanelState, WithGlicApi} from 'chrome://glic/glic_api/glic_api.js';

// Glic API bootstrap code.
function createGlicHostRegistryOnLoad(): Promise<GlicHostRegistry> {
  return new Promise<GlicHostRegistry>(resolve => {
    function messageHandler(event: MessageEvent) {
      // Important: only accept messages from Chrome browser's glic WebUI.
      if (event.origin !== 'chrome://glic' || event.source === null) {
        return;
      }
      const bootMessage = event.data as GlicApiBootMessage;
      if (bootMessage?.type === 'glic-bootstrap' && bootMessage.glicApiSource) {
        window.removeEventListener('message', messageHandler);
        const glicApiSource = bootMessage.glicApiSource;
        const scriptElement = document.createElement('script');
        scriptElement.textContent = glicApiSource;
        document.head.appendChild(scriptElement);
        const bootFunc = (window as WithGlicApi).internalAutoGlicBoot;
        if (!bootFunc) {
          throw new Error('Glic client import failed.');
        }
        resolve(bootFunc(event.source as WindowProxy));
      }
    }
    window.addEventListener('message', messageHandler);
  });
}

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
