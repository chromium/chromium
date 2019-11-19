// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-kiosk-dialog. */

import {KioskBrowserProxyImpl} from 'chrome://extensions/extensions.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {flushTasks} from '../test_util.m.js';
import {TestKioskBrowserProxy} from './test_kiosk_browser_proxy.js';

window.extension_kiosk_mode_tests = {};
extension_kiosk_mode_tests.suiteName = 'kioskModeTests';
/** @enum {string} */
extension_kiosk_mode_tests.TestNames = {
  AddButton: 'AddButton',
  AddError: 'AddError',
  AutoLaunch: 'AutoLaunch',
  Bailout: 'Bailout',
  Layout: 'Layout',
  Updated: 'Updated',
};

suite(extension_kiosk_mode_tests.suiteName, function() {
  /** @type {KioskBrowserProxy} */
  let browserProxy;

  /** @type {ExtensionsKioskDialogElement} */
  let dialog;

  /** @type {!Array<!KioskApp>} */
  const basicApps = [
    {
      id: 'app_1',
      name: 'App1 Name',
      iconURL: '',
      autoLaunch: false,
      isLoading: false,
    },
    {
      id: 'app_2',
      name: 'App2 Name',
      iconURL: '',
      autoLaunch: false,
      isLoading: false,
    },
  ];

  /** @param {!KioskAppSettings} */
  function setAppSettings(settings) {
    const appSettings = {
      apps: [],
      disableBailout: false,
      hasAutoLaunchApp: false,
    };

    browserProxy.setAppSettings(Object.assign({}, appSettings, settings));
  }

  /** @param {!KioskSettings} */
  function setInitialSettings(settings) {
    const initialSettings = {
      kioskEnabled: true,
      autoLaunchEnabled: false,
    };

    browserProxy.setInitialSettings(
        Object.assign({}, initialSettings, settings));
  }

  /** @return {!Promise} */
  function initPage() {
    PolymerTest.clearBody();
    browserProxy.reset();
    dialog = document.createElement('extensions-kiosk-dialog');
    document.body.appendChild(dialog);

    return browserProxy.whenCalled('getKioskAppSettings')
        .then(() => flushTasks());
  }

  setup(function() {
    browserProxy = new TestKioskBrowserProxy();
    setAppSettings({apps: basicApps.slice(0)});
    KioskBrowserProxyImpl.instance_ = browserProxy;

    return initPage();
  });

  test(assert(extension_kiosk_mode_tests.TestNames.Layout), function() {
    const apps = basicApps.slice(0);
    apps[1].autoLaunch = true;
    apps[1].isLoading = true;
    setAppSettings({apps: apps, hasAutoLaunchApp: true});

    return initPage()
        .then(() => {
          const items = dialog.shadowRoot.querySelectorAll('.list-item');
          expectEquals(items.length, 2);
          expectTrue(items[0].textContent.includes(basicApps[0].name));
          expectTrue(items[1].textContent.includes(basicApps[1].name));
          // Second item should show the auto-lauch label.
          expectTrue(items[0].querySelector('span').hidden);
          expectFalse(items[1].querySelector('span').hidden);
          // No permission to edit auto-launch so buttons should be hidden.
          expectTrue(items[0].querySelector('cr-button').hidden);
          expectTrue(items[1].querySelector('cr-button').hidden);
          // Bailout checkbox should be hidden when auto-launch editing
          // disabled.
          expectTrue(dialog.$$('cr-checkbox').hidden);

          items[0].querySelector('.icon-delete-gray').click();
          flush();
          return browserProxy.whenCalled('removeKioskApp');
        })
        .then(appId => {
          expectEquals(appId, basicApps[0].id);
        });
  });

  test(assert(extension_kiosk_mode_tests.TestNames.AutoLaunch), function() {
    const apps = basicApps.slice(0);
    apps[1].autoLaunch = true;
    setAppSettings({apps: apps, hasAutoLaunchApp: true});
    setInitialSettings({autoLaunchEnabled: true});

    let buttons;
    return initPage()
        .then(() => {
          buttons = dialog.shadowRoot.querySelectorAll('.list-item cr-button');
          // Has permission to edit auto-launch so buttons should be seen.
          expectFalse(buttons[0].hidden);
          expectFalse(buttons[1].hidden);

          buttons[0].click();
          return browserProxy.whenCalled('enableKioskAutoLaunch');
        })
        .then(appId => {
          expectEquals(appId, basicApps[0].id);

          buttons[1].click();
          return browserProxy.whenCalled('disableKioskAutoLaunch');
        })
        .then(appId => {
          expectEquals(appId, basicApps[1].id);
        });
  });

  test(assert(extension_kiosk_mode_tests.TestNames.Bailout), function() {
    const apps = basicApps.slice(0);
    apps[1].autoLaunch = true;
    setAppSettings({apps: apps, hasAutoLaunchApp: true});
    setInitialSettings({autoLaunchEnabled: true});

    expectFalse(dialog.$['confirm-dialog'].open);

    let bailoutCheckbox;
    return initPage()
        .then(() => {
          bailoutCheckbox = dialog.$$('cr-checkbox');
          // Bailout checkbox should be usable when auto-launching.
          expectFalse(bailoutCheckbox.hidden);
          expectFalse(bailoutCheckbox.disabled);
          expectFalse(bailoutCheckbox.checked);

          // Making sure canceling doesn't change anything.
          bailoutCheckbox.click();
          flush();
          expectTrue(dialog.$['confirm-dialog'].open);

          dialog.$['confirm-dialog'].querySelector('.cancel-button').click();
          flush();
          expectFalse(bailoutCheckbox.checked);
          expectFalse(dialog.$['confirm-dialog'].open);
          expectTrue(dialog.$.dialog.open);

          // Accepting confirmation dialog should trigger browserProxy call.
          bailoutCheckbox.click();
          flush();
          expectTrue(dialog.$['confirm-dialog'].open);

          dialog.$['confirm-dialog'].querySelector('.action-button').click();
          flush();
          expectTrue(bailoutCheckbox.checked);
          expectFalse(dialog.$['confirm-dialog'].open);
          expectTrue(dialog.$.dialog.open);
          return browserProxy.whenCalled('setDisableBailoutShortcut');
        })
        .then(disabled => {
          expectTrue(disabled);

          // Test clicking on checkbox again should simply re-enable bailout.
          browserProxy.reset();
          bailoutCheckbox.click();
          expectFalse(bailoutCheckbox.checked);
          expectFalse(dialog.$['confirm-dialog'].open);
          return browserProxy.whenCalled('setDisableBailoutShortcut');
        })
        .then(disabled => {
          expectFalse(disabled);
        });
  });

  test(assert(extension_kiosk_mode_tests.TestNames.AddButton), function() {
    const addButton = dialog.$['add-button'];
    expectTrue(!!addButton);
    expectTrue(addButton.disabled);

    const addInput = dialog.$['add-input'];
    addInput.value = 'blah';
    expectFalse(addButton.disabled);

    addButton.click();
    return browserProxy.whenCalled('addKioskApp').then(appId => {
      expectEquals(appId, 'blah');
    });
  });

  test(assert(extension_kiosk_mode_tests.TestNames.Updated), function() {
    const items = dialog.shadowRoot.querySelectorAll('.list-item');
    expectTrue(items[0].textContent.includes(basicApps[0].name));

    const newName = 'completely different name';

    window.cr.webUIListenerCallback('kiosk-app-updated', {
      id: basicApps[0].id,
      name: newName,
      iconURL: '',
      autoLaunch: false,
      isLoading: false,
    });

    expectFalse(items[0].textContent.includes(basicApps[0].name));
    expectTrue(items[0].textContent.includes(newName));
  });

  test(assert(extension_kiosk_mode_tests.TestNames.AddError), function() {
    const addInput = dialog.$['add-input'];

    expectFalse(!!addInput.invalid);
    window.cr.webUIListenerCallback('kiosk-app-error', basicApps[0].id);

    expectTrue(!!addInput.invalid);
    expectTrue(addInput.errorMessage.includes(basicApps[0].id));
  });
});
