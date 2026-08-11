// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {GlicBrowserHost, GlicWebClient, InvokeOptions, TabData} from '/glic/glic_api/glic_api.js';

import {createGlicHostRegistryOnLoad} from '../api_boot.js';

declare global {
  interface Window {
    glicTestClient?: GlicTestWebClient;
  }
}

export class GlicTestWebClient implements GlicWebClient {
  browserHost: GlicBrowserHost|null = null;
  lastPrompt: string = '';
  focusedTabUrl: string = '';
  private promptResolve?: (prompt: string) => void;
  private promptPromise: Promise<string> = new Promise((resolve) => {
    this.promptResolve = resolve;
  });

  async initialize(browser: GlicBrowserHost): Promise<void> {
    this.browserHost = browser;
  }

  async invoke(options: InvokeOptions): Promise<void> {
    this.lastPrompt = options.prompts?.[0] || '';
    if (this.promptResolve) {
      this.promptResolve(this.lastPrompt);
    }
  }

  async waitForPrompt(): Promise<string> {
    if (this.lastPrompt) {
      return this.lastPrompt;
    }
    return this.promptPromise;
  }

  async getContextFromFocusedTabUrl(): Promise<string> {
    if (!this.browserHost) {
      return 'error: no browserHost';
    }
    if (!this.browserHost.getContextFromFocusedTab) {
      return 'error: getContextFromFocusedTab is not supported';
    }
    try {
      const result = await this.browserHost.getContextFromFocusedTab(
          {viewportScreenshot: false});
      const tabData: TabData|undefined = result?.tabData;
      this.focusedTabUrl = tabData?.url || 'null result';
      return this.focusedTabUrl;
    } catch (e) {
      this.focusedTabUrl = `error: ${e}`;
      return this.focusedTabUrl;
    }
  }
}

const client = new GlicTestWebClient();
window.glicTestClient = client;

createGlicHostRegistryOnLoad().then((registry) => {
  registry.registerWebClient(client);
});
