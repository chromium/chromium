// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/default_browser/app_refresh.js';

import type {AppRefreshElement} from 'chrome://intro/default_browser/app_refresh.js';
import type {DefaultBrowserBrowserProxy} from 'chrome://intro/default_browser/browser_proxy.js';
import {DefaultBrowserBrowserProxyImpl} from 'chrome://intro/default_browser/browser_proxy.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

class TestDefaultBrowserBrowserProxy extends TestBrowserProxy implements
    DefaultBrowserBrowserProxy {
  constructor() {
    super([
      'setAsDefaultBrowser',
      'skipDefaultBrowser',
    ]);
  }

  setAsDefaultBrowser() {
    this.methodCalled('setAsDefaultBrowser');
  }

  skipDefaultBrowser() {
    this.methodCalled('skipDefaultBrowser');
  }
}

suite('DefaultBrowserRefreshTest', function() {
  let app: AppRefreshElement;
  let browserProxy: TestDefaultBrowserBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestDefaultBrowserBrowserProxy();
    DefaultBrowserBrowserProxyImpl.setInstance(browserProxy);
    app = document.createElement('default-browser-app-refresh');
    document.body.appendChild(app);
  });

  test('ConfirmButtonClick', async function() {
    const confirmButton =
        app.shadowRoot.querySelector<HTMLButtonElement>('#confirm-button');
    assertTrue(!!confirmButton);
    assertFalse(confirmButton.disabled);

    confirmButton.click();
    await browserProxy.whenCalled('setAsDefaultBrowser');

    await microtasksFinished();
    assertTrue(confirmButton.disabled);
    const skipButton =
        app.shadowRoot.querySelector<HTMLButtonElement>('#skip-button');
    assertTrue(!!skipButton);
    assertTrue(skipButton.disabled);
  });

  test('SkipButtonClick', async function() {
    const skipButton =
        app.shadowRoot.querySelector<HTMLButtonElement>('#skip-button');
    assertTrue(!!skipButton);
    assertFalse(skipButton.disabled);

    skipButton.click();
    await browserProxy.whenCalled('skipDefaultBrowser');

    await microtasksFinished();
    assertTrue(skipButton.disabled);
    const confirmButton =
        app.shadowRoot.querySelector<HTMLButtonElement>('#confirm-button');
    assertTrue(!!confirmButton);
    assertTrue(confirmButton.disabled);
  });

  test('ResetButtons', async function() {
    const confirmButton =
        app.shadowRoot.querySelector<HTMLButtonElement>('#confirm-button');
    assertTrue(!!confirmButton);

    confirmButton.click();
    await browserProxy.whenCalled('setAsDefaultBrowser');
    await microtasksFinished();
    assertTrue(confirmButton.disabled);

    webUIListenerCallback('reset-default-browser-buttons');
    await microtasksFinished();

    assertFalse(confirmButton.disabled);
    const skipButton =
        app.shadowRoot.querySelector<HTMLButtonElement>('#skip-button');
    assertTrue(!!skipButton);
    assertFalse(skipButton.disabled);
  });
});
