// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LockScreenUnlockType, LockStateMixin, QuickUnlockBrowserProxyImpl} from 'chrome://os-settings/os_settings.js';
import type {PrefsState, QuickUnlockBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {FingerprintBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {PolymerElement, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {TestFingerprintBrowserProxy} from './os_people_page/test_fingerprint_browser_proxy.js';

// Dummy element to host the mixin for testing
const TestElementBase = LockStateMixin(PolymerElement);
class TestElement extends TestElementBase {
  static get is() {
    return 'test-element';
  }

  static get template() {
    return html`<div>[[unlockStatusLabel_]]</div>`;
  }

  prefs: PrefsState = {};
}
customElements.define(TestElement.is, TestElement);

class TestQuickUnlockBrowserProxy extends TestBrowserProxy implements
    QuickUnlockBrowserProxy {
  private activeFactors: {password: boolean, pin: boolean} = {
    password: false,
    pin: false,
  };

  constructor() {
    super([
      'requestActiveAuthFactors',
    ]);
  }

  setActiveFactors(password: boolean, pin: boolean) {
    this.activeFactors = {password, pin};
  }

  requestActiveAuthFactors() {
    this.methodCalled('requestActiveAuthFactors');
    return Promise.resolve(this.activeFactors);
  }
}

suite('LockStateMixinTests', () => {
  let testElement: TestElement;
  let fingerprintBrowserProxy: TestFingerprintBrowserProxy;
  let quickUnlockBrowserProxy: TestQuickUnlockBrowserProxy;

  setup(async () => {
    fingerprintBrowserProxy = new TestFingerprintBrowserProxy();
    FingerprintBrowserProxyImpl.setInstanceForTesting(fingerprintBrowserProxy);

    quickUnlockBrowserProxy = new TestQuickUnlockBrowserProxy();
    QuickUnlockBrowserProxyImpl.setInstance(quickUnlockBrowserProxy);

    // Set up default mock responses
    fingerprintBrowserProxy.setFingerprints([]);
    // Password only by default.
    quickUnlockBrowserProxy.setActiveFactors(true, false);

    testElement = document.createElement('test-element') as TestElement;
    // Ensure the pref is enabled initially
    testElement.prefs = {
      settings: {
        enable_screen_lock: {
          value: true,
        },
      },
    };
    document.body.appendChild(testElement);

    // Initialize the lock state
    testElement.initializeLockState();
    await flushTasks();
  });

  teardown(() => {
    testElement.remove();
  });

  test('Screen Lock label updates when toggled off and on', async () => {
    // Initial state: screen lock enabled, password configured.
    // LockStateMixin should determine type as PASSWORD.
    assertEquals(LockScreenUnlockType.PASSWORD, testElement.selectedUnlockType);

    // Now toggle screen lock off (simulating pref change)
    testElement.set('prefs.settings.enable_screen_lock.value', false);
    webUIListenerCallback('settings.enable_screen_lock.changed', false);
    await flushTasks();

    // Now type should be LOCK_SCREEN_NONE
    assertEquals(LockScreenUnlockType.LOCK_SCREEN_NONE, testElement.selectedUnlockType);

    // Now toggle screen lock back on
    testElement.set('prefs.settings.enable_screen_lock.value', true);
    webUIListenerCallback('settings.enable_screen_lock.changed', true);
    await flushTasks();

    // Now type should be PASSWORD again
    assertEquals(LockScreenUnlockType.PASSWORD, testElement.selectedUnlockType);
  });
});
