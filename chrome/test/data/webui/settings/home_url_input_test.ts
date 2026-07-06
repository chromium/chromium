// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AppearanceBrowserProxyImpl, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import type {HomeUrlInputElement} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestAppearanceBrowserProxy} from './test_appearance_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

suite('HomeUrlInput', function() {
  let appearanceBrowserProxy: TestAppearanceBrowserProxy;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let homeUrlInput: HomeUrlInputElement;

  const initialPrefs = [
    {
      key: 'homepage',
      type: chrome.settingsPrivate.PrefType.URL,
      value: 'http://chromium.org',
    },
  ];

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    appearanceBrowserProxy = new TestAppearanceBrowserProxy();
    AppearanceBrowserProxyImpl.setInstance(appearanceBrowserProxy);

    prefsBrowserProxy = new TestPrefsBrowserProxy(initialPrefs);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);

    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    homeUrlInput = document.createElement('home-url-input');
    document.body.appendChild(homeUrlInput);
    await microtasksFinished();
  });

  test('UserInputUpdatesPref', async function() {
    assertEquals('http://chromium.org', homeUrlInput.value);

    // Simulate user input.
    homeUrlInput.$.input.value = 'http://google.com';
    await microtasksFinished();
    homeUrlInput.$.input.dispatchEvent(new Event('change'));

    // Verify pref is updated.
    assertEquals(
        'http://google.com',
        PrefService.getInstance().getPref('homepage').value);
  });
});
