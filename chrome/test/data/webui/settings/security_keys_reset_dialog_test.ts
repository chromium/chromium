// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import type {SecurityKeysResetBrowserProxy, SettingsSecurityKeysResetDialogElement} from 'chrome://settings/lazy_load.js';
import {ResetDialogPage, SecurityKeysResetBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {assertShown} from './security_keys_test_util.js';
import {TestSecurityKeysBrowserProxy} from './test_security_keys_browser_proxy.js';

class TestSecurityKeysResetBrowserProxy extends TestSecurityKeysBrowserProxy
    implements SecurityKeysResetBrowserProxy {
  constructor() {
    super([
      'reset',
      'completeReset',
      'close',
    ]);
  }

  override reset() {
    return this.handleMethod('reset');
  }

  completeReset() {
    return this.handleMethod('completeReset');
  }

  close() {
    this.methodCalled('close');
  }
}

suite('SecurityKeysResetDialog', function() {
  let dialog: SettingsSecurityKeysResetDialogElement;
  let allDivs: string[];
  let browserProxy: TestSecurityKeysResetBrowserProxy;

  setup(function() {
    browserProxy = new TestSecurityKeysResetBrowserProxy();
    SecurityKeysResetBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('settings-security-keys-reset-dialog');
    allDivs = Object.values(ResetDialogPage);
  });

  function assertComplete() {
    assertEquals(dialog.$.button.textContent!.trim(), 'OK');
    assertEquals(dialog.$.button.className, 'action-button');
  }

  function assertNotComplete() {
    assertEquals(dialog.$.button.textContent!.trim(), 'Cancel');
    assertEquals(dialog.$.button.className, 'cancel-button');
  }

  test('Initialization', async function() {
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('reset');
    assertShown(allDivs, dialog, 'initial');
    assertNotComplete();
  });

  test('Cancel', async function() {
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('reset');
    assertShown(allDivs, dialog, 'initial');
    assertNotComplete();
    dialog.$.button.click();
    await browserProxy.whenCalled('close');
    assertFalse(dialog.$.dialog.open);
  });

  test('NotSupported', async function() {
    browserProxy.setResponseFor(
        'reset', Promise.resolve(1 /* INVALID_COMMAND */));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown(allDivs, dialog, 'noReset');
  });

  test('ImmediateUnknownError', async function() {
    const error = 1000 /* undefined error code */;
    browserProxy.setResponseFor('reset', Promise.resolve(error));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown(allDivs, dialog, 'resetFailed');
    assertTrue(
        dialog.$.resetFailed.textContent!.trim().includes(error.toString()));
  });

  test('ImmediateUnknownError', async function() {
    browserProxy.setResponseFor('reset', Promise.resolve(0 /* success */));
    const promiseResolver = new PromiseResolver();
    browserProxy.setResponseFor('completeReset', promiseResolver.promise);
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('completeReset');
    assertNotComplete();
    assertShown(allDivs, dialog, 'resetConfirm');
    promiseResolver.resolve(0 /* success */);
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown(allDivs, dialog, 'resetSuccess');
  });

  test('UnknownError', async function() {
    const error = 1000 /* undefined error code */;
    browserProxy.setResponseFor('reset', Promise.resolve(0 /* success */));
    browserProxy.setResponseFor('completeReset', Promise.resolve(error));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('completeReset');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown(allDivs, dialog, 'resetFailed');
    assertTrue(
        dialog.$.resetFailed.textContent!.trim().includes(error.toString()));
  });

  test('ResetRejected', async function() {
    browserProxy.setResponseFor('reset', Promise.resolve(0 /* success */));
    browserProxy.setResponseFor(
        'completeReset', Promise.resolve(48 /* NOT_ALLOWED */));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('completeReset');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown(allDivs, dialog, 'resetNotAllowed');
  });
});
