// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserProxy} from 'chrome://glic/browser_proxy.js';
import type {PageHandlerInterface} from 'chrome://glic/glic.mojom-webui.js';
import type {ApiHostEmbedder} from 'chrome://glic/glic_api_impl/host/glic_api_host.js';
import type {PageType, WebviewDelegate} from 'chrome://glic/webview.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

/**
 * Configures loadTimeData with default values for tests.
 * @param overrides Optional overrides for specific test cases.
 */
export function configureLoadTimeData(overrides: Record<string, any> = {}) {
  loadTimeData.resetForTesting(Object.assign(
      {
        glicAllowedOrigins: '',
        glicGuestURL: 'https://cat.fun/',
        devMode: false,
        chromeVersion: '123.0.0.0',
        chromeChannel: 'stable',
        glicHeaderRequestTypes: '',
        zoomLabel: 'Zoom: $1',
      },
      overrides));
}

export class FakePageHandler implements Partial<PageHandlerInterface> {
  webviewCommitted(_url: string) {}
  prepareForClient() {
    return Promise.resolve({result: 0});
  }
}

export class FakeBrowserProxy implements BrowserProxy {
  pageHandler = new FakePageHandler() as PageHandlerInterface;
}

export class FakeWebviewDelegate implements WebviewDelegate {
  webviewError(_reason: string) {}
  webviewUnresponsive() {}
  webviewPageCommit(_pageType: PageType) {}
  webviewDeniedByAdmin() {}
}

export class FakeApiHostEmbedder implements ApiHostEmbedder {
  enableDragResize(_enabled: boolean) {}
  webClientReady() {}
  webClientWarmed() {}
}
