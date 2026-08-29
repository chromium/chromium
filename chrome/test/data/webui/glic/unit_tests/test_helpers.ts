// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserProxy, PageHandlerInterface, WebviewDelegate} from 'chrome://glic/glic.js';
import {PageCallbackRouter, PreloadPageCallbackRouter} from 'chrome://glic/glic.js';
import type {GuestPageType} from 'chrome://glic/glic.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

/**
 * Configures loadTimeData with default values for tests.
 * @param overrides Optional overrides for specific test cases.
 */
export function configureLoadTimeData(overrides: Record<string, any> = {}) {
  loadTimeData.resetForTesting(Object.assign(
      {
        glicGuestURL: 'https://cat.fun/',
        devMode: false,
        chromeVersion: '123.0.0.0',
        chromeChannel: 'stable',
        glicHeaderRequestTypes: '',
        zoomLabel: 'Zoom: $1',
        loggingEnabled: false,
        maxInFlightRequests: 10,
        sendResponsesForAllRequests: false,
        glicGuestAPISource: '',
        enableStructuredYieldMetadata: false,
        glicPopupWindowsEnabled: false,
      },
      overrides));
}

export class FakePageHandler implements Partial<PageHandlerInterface> {
  createWebClient(_receiver: any) {}
  webviewCommitted(_url: any) {}
  onZoomLevelChange(_zoomFactor: number) {}
  prepareForClient() {
    return Promise.resolve({result: 0});
  }
}

export class FakeBrowserProxy implements BrowserProxy {
  pageHandler = new FakePageHandler() as unknown as PageHandlerInterface;
  pageCallbackRouter = new PageCallbackRouter();
  preloadPageCallbackRouter = new PreloadPageCallbackRouter();
}

export class FakeWebviewDelegate implements WebviewDelegate {
  webviewError(_reason: string) {}
  webviewUnresponsive() {}
  webviewPageCommit(_pageType?: GuestPageType, _isApiAllowed?: boolean) {}
  webviewDeniedByAdmin() {}
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
