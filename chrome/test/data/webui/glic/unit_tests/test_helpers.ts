// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ApiHostEmbedder, BrowserProxy, PageHandlerInterface, PageType, WebviewDelegate} from 'chrome://glic/glic.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

/**
 * Configures loadTimeData with default values for tests.
 * @param overrides Optional overrides for specific test cases.
 */
export function configureLoadTimeData(overrides: Record<string, any> = {}) {
  loadTimeData.resetForTesting(Object.assign(
      {
        glicAllowedOrigins: '',
        glicApiAllowedOrigins: '',
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
  onZoomLevelChange(_zoomFactor: number) {}
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
  getZoom(): Promise<number> {
    return Promise.resolve(1.0);
  }
}

export function assertDeepEquals(a: unknown, b: unknown): void {
  assertEquals(JSON.stringify(a), JSON.stringify(b));
}

export function sleep(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}

export async function assertRejects<T>(
    promise: Promise<T>,
    options?: {withErrorMessage?: string}): Promise<string|undefined> {
  return promise.then(
      () => {
        // The promise should have been rejected.
        throw new Error('Promise not rejected.');
      },
      (e) => {
        const errorMessage = (e as Error).message;
        if (options?.withErrorMessage !== undefined) {
          assertEquals(options.withErrorMessage, errorMessage);
        }
        return errorMessage;
      });
}

export async function waitUntilEqual<T>(
    getter: () => T, value: T, maxMs: number = 1000): Promise<void> {
  const startTime = performance.now();
  while (getter() !== value) {
    if (performance.now() - startTime > maxMs) {
      throw new Error(
          'Timed out waiting for ' + JSON.stringify(value) + ' to be returned');
    }
    await sleep(10);
  }
}
