// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {setAppNotificationProviderForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://test/test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

class FakeAppNotificationHandler {
  constructor() {
    /** @private {!Map<string, !PromiseResolver>} */
    this.resolverMap_ = new Map();

    /**
     * @private
     *     {?chromeos.settings.appNotification.mojom.
     *      AppNotificationObserverRemote}
     */
    this.appNotificationObserverRemote_;

    this.isQuietModeEnabled_ = false;

    this.resetForTest();
  }

  resetForTest() {
    if (this.appNotificationObserverRemote_) {
      this.appNotificationObserverRemote_ = null;
    }

    this.resolverMap_.set('addObserver', new PromiseResolver());
    this.resolverMap_.set('setQuietMode', new PromiseResolver());
    this.resolverMap_.set('notifyPageReady', new PromiseResolver());
  }

  /**
   * @param {string} methodName
   * @return {!PromiseResolver}
   * @private
   */
  getResolver_(methodName) {
    const method = this.resolverMap_.get(methodName);
    assertTrue(!!method, `Method '${methodName}' not found.`);
    return method;
  }

  /**
   * @param {string} methodName
   * @protected
   */
  methodCalled(methodName) {
    this.getResolver_(methodName).resolve();
  }

  /**
   * @param {string} methodName
   * @return {!Promise}
   */
  whenCalled(methodName) {
    return this.getResolver_(methodName).promise.then(() => {
      // Support sequential calls to whenCalled by replacing the promise.
      this.resolverMap_.set(methodName, new PromiseResolver());
    });
  }

  /**
   * @return
   *      {chromeos.settings.appNotification.mojom.
   *        AppNotificationObserverRemote}
   */
  getObserverRemote() {
    return this.appNotificationObserverRemote_;
  }

  /**
   * @return {boolean} quietModeState
   */
  getCurrentQuietModeState() {
    return this.isQuietModeEnabled_;
  }

  // appNotificationHandler methods

  /**
   * @param {!chromeos.settings.appNotification.mojom.
   *        AppNotificationObserverRemote}
   *      remote
   * @return {!Promise}
   */
  addObserver(remote) {
    return new Promise(resolve => {
      this.appNotificationObserverRemote_ = remote;
      this.methodCalled('addObserver');
      resolve();
    });
  }

  /** @return {!Promise<{success: boolean}>} */
  setQuietMode(enabled) {
    this.isQuietModeEnabled_ = enabled;
    return new Promise(resolve => {
      this.methodCalled('setQuietMode');
      resolve({success: true});
    });
  }

  /** @return {!Promise} */
  notifyPageReady() {
    return new Promise(resolve => {
      this.methodCalled('notifyPageReady');
      resolve();
    });
  }
}


suite('AppNotificationsSubpageTests', function() {
  /** @type {AppNotificationsSubpage} */
  let page;

  /**
   * @type {
   *    ?chromeos.settings.appNotification.mojom.AppNotificationHandlerRemote
   *  }
   */
  let mojoApi_;

  let setQuietModeCounter = 0;

  suiteSetup(() => {
    mojoApi_ = new FakeAppNotificationHandler();
    setAppNotificationProviderForTesting(mojoApi_);
  });

  setup(function() {
    PolymerTest.clearBody;
    loadTimeData.overrideValues({showOsSettingsAppNotificationsRow: true});
    page = document.createElement('settings-app-notifications-subpage');
    document.body.appendChild(page);
    assertTrue(!!page);
    flush();
  });

  teardown(function() {
    mojoApi_.resetForTest();
    page.remove();
    page = null;
  });

  /**
   * @return {!Promise}
   */
  function initializeObserver() {
    return mojoApi_.whenCalled('addObserver');
  }

  function simulateQuickSettings(enable) {
    mojoApi_.getObserverRemote().onQuietModeChanged(enable);
  }

  // TODO(crbug/1240175): Ensure that CrToggle is disabled correctly and
  // CrPolicyIndicator is hidden correctly.
  test('Each app-notification-row displays correctly', function() {
    const chromeRow = page.$.appNotificationsList.children[0];
    const filesRow = page.$.appNotificationsList.children[1];

    assertTrue(!!page);
    flush();

    assertEquals('Chrome', chromeRow.$.appTitle.textContent.trim());
    assertTrue(chromeRow.$.appToggle.disabled);
    assertTrue(!!chromeRow.shadowRoot.querySelector('cr-policy-indicator'));

    assertEquals('Files', filesRow.$.appTitle.textContent.trim());
    assertFalse(filesRow.$.appToggle.disabled);
    assertFalse(!!filesRow.shadowRoot.querySelector('cr-policy-indicator'));
  });

  test('toggleDoNotDisturb', function() {
    const div = page.$.enableDoNotDisturb;
    const dndToggle = page.$.enableDndToggle;

    flush();
    assertFalse(dndToggle.checked);
    assertFalse(mojoApi_.getCurrentQuietModeState());

    // Test that tapping the single settings-box div enables DND.
    assertTrue(!!div);
    div.click();
    assertTrue(dndToggle.checked);
    assertTrue(mojoApi_.getCurrentQuietModeState());

    // Click again will change the value.
    div.click();
    assertFalse(dndToggle.checked);
    assertFalse(mojoApi_.getCurrentQuietModeState());

    // Test that tapping the toggle itself enables DND.
    dndToggle.click();
    assertTrue(dndToggle.checked);
    assertTrue(mojoApi_.getCurrentQuietModeState());
  });

  test('SetQuietMode being called correctly', function() {
    const dndToggle = page.$.enableDndToggle;
    // Click the toggle button a certain count.
    const testCount = 3;

    flush();
    assertFalse(dndToggle.checked);

    // Verify that every toggle click makes a call to setQuietMode and is
    // counted accordingly.
    for (let i = 0; i < testCount; i++) {
      return initializeObserver()
          .then(() => {
            flush();
            dndToggle.click();
            return mojoApi_.whenCalled('setQuietMode').then(() => {
              setQuietModeCounter++;
            });
          })
          .then(() => {
            flush();
            assertEquals(i + 1, setQuietModeCounter);
          });
    }
  });

  test('changing toggle with OnQuietModeChanged', function() {
    const dndToggle = page.$.enableDndToggle;

    flush();
    assertFalse(dndToggle.checked);

    // Verify that calling onQuietModeChanged sets toggle value.
    // This is equivalent to changing the DoNotDisturb setting in quick
    // settings.
    return initializeObserver()
        .then(() => {
          flush();
          simulateQuickSettings(/** enable */ true);
          return flushTasks();
        })
        .then(() => {
          assertTrue(dndToggle.checked);
          assertTrue(mojoApi_.getCurrentQuietModeState());
        });
  });
});
