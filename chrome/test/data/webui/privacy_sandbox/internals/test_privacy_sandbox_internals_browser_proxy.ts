// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerRemote, PrivacySandboxInternalsPref} from 'chrome://privacy-sandbox-internals/privacy_sandbox_internals.mojom-webui.js';
import type {PrivacySandboxInternalsBrowserProxy} from 'chrome://privacy-sandbox-internals/privacy_sandbox_internals_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPrivacySandboxInternalsPageHandler extends TestBrowserProxy {
  private prefs_: PrivacySandboxInternalsPref[] = [];

  constructor() {
    super([
      'readPrefsWithPrefixes',
      'readContentSettings',
      'getTpcdMetadataGrants',
    ]);
  }

  setPrefs(prefs: PrivacySandboxInternalsPref[]) {
    this.prefs_ = prefs;
  }

  readPrefsWithPrefixes() {
    this.methodCalled('readPrefsWithPrefixes');
    return Promise.resolve({prefs: this.prefs_});
  }

  readContentSettings() {
    this.methodCalled('readContentSettings');
    return Promise.resolve({contentSettings: []});
  }

  getTpcdMetadataGrants() {
    this.methodCalled('getTpcdMetadataGrants');
    return Promise.resolve({contentSettings: []});
  }
}

export class TestPrivacySandboxInternalsBrowserProxy implements
    PrivacySandboxInternalsBrowserProxy {
  handler: PageHandlerRemote;
  testHandler: TestPrivacySandboxInternalsPageHandler;
  private shouldShowDevUi_: boolean = false;

  constructor() {
    this.testHandler = new TestPrivacySandboxInternalsPageHandler();
    this.handler = this.testHandler as unknown as PageHandlerRemote;
  }

  setShouldShowTpcdMetadataGrants(isEnabled: boolean) {
    this.shouldShowDevUi_ = isEnabled;
  }

  shouldShowTpcdMetadataGrants(): boolean {
    return this.shouldShowDevUi_;
  }
}
