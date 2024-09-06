// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {AccountManagerBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs, OsSettingsUiElement, Router, routes, setNearbyShareSettingsForTesting, setUserActionRecorderForTesting} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_share_settings.js';

import {FakeUserActionRecorder} from '../fake_user_action_recorder.js';
import {TestAccountManagerBrowserProxy} from '../os_people_page/test_account_manager_browser_proxy.js';

suite('User action recorder', () => {
  let ui: OsSettingsUiElement;
  let fakeUserActionRecorder: FakeUserActionRecorder;
  let fakeNearbySettings: FakeNearbyShareSettings;
  let testAccountManagerBrowserProxy: TestAccountManagerBrowserProxy;

  async function createElement(): Promise<OsSettingsUiElement> {
    const element = document.createElement('os-settings-ui');
    document.body.appendChild(element);
    await CrSettingsPrefs.initialized;
    flush();
    return element;
  }

  suiteSetup(() => {
    fakeNearbySettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeNearbySettings);

    // Setup fake accounts.
    testAccountManagerBrowserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(
        testAccountManagerBrowserProxy);
  });

  setup(async () => {
    fakeUserActionRecorder = new FakeUserActionRecorder();
    setUserActionRecorderForTesting(fakeUserActionRecorder);
    ui = await createElement();
  });

  teardown(() => {
    ui.remove();
    Router.getInstance().resetRouteForTesting();
    testAccountManagerBrowserProxy.reset();
  });

  test('Records navigation changes', () => {
    assertEquals(0, fakeUserActionRecorder.navigationCount);

    Router.getInstance().navigateTo(routes.INTERNET);
    assertEquals(1, fakeUserActionRecorder.navigationCount);

    Router.getInstance().navigateTo(routes.BASIC);
    assertEquals(2, fakeUserActionRecorder.navigationCount);
  });

  test('Records blur events', () => {
    assertEquals(0, fakeUserActionRecorder.pageBlurCount);
    window.dispatchEvent(new Event('blur'));
    assertEquals(1, fakeUserActionRecorder.pageBlurCount);
  });

  test('Records click events', () => {
    assertEquals(0, fakeUserActionRecorder.clickCount);
    ui.click();
    assertEquals(1, fakeUserActionRecorder.clickCount);
  });

  test('Records focus events', () => {
    // Focus is already recorded when the page is loaded
    assertEquals(1, fakeUserActionRecorder.pageFocusCount);
    window.dispatchEvent(new Event('focus'));
    assertEquals(2, fakeUserActionRecorder.pageFocusCount);
  });

  test('Records settings changes', () => {
    assertEquals(0, fakeUserActionRecorder.settingChangeCount);
    const prefsEl = ui.shadowRoot!.querySelector('#prefs');
    assertTrue(!!prefsEl);
    prefsEl.dispatchEvent(new CustomEvent('user-action-setting-change', {
      bubbles: true,
      composed: true,
      detail: {prefKey: 'foo', prefValue: 'bar'},
    }));
    assertEquals(1, fakeUserActionRecorder.settingChangeCount);
  });
});
