// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {AmbientModeTopicSource, AmbientModeTemperatureUnit, AmbientModeBrowserProxyImpl, CrSettingsPrefs, routes, Router} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {TestBrowserProxy} from '../../test_browser_proxy.m.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

/**
 * @implements {settings.AmbientModeBrowserProxy}
 */
class TestAmbientModeBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'requestSettings',
      'requestAlbums',
      'setSelectedTemperatureUnit',
      'setSelectedAlbums',
    ]);
  }

  /** @override */
  requestSettings() {
    this.methodCalled('requestSettings');
  }

  /** @override */
  requestAlbums(topicSource) {
    this.methodCalled('requestAlbums', [topicSource]);
  }

  /** @override */
  setSelectedTemperatureUnit(temperatureUnit) {
    this.methodCalled('setSelectedTemperatureUnit', [temperatureUnit]);
  }

  /** @override */
  setSelectedAlbums(settings) {
    this.methodCalled('setSelectedAlbums', [settings]);
  }
}

suite('AmbientModeHandler', function() {
  /** @type {SettingsAmbientModePageElement} */
  let ambientModePage = null;

  /** @type {?TestAmbientModeBrowserProxy} */
  let browserProxy = null;

  suiteSetup(function() {});

  setup(function() {
    browserProxy = new TestAmbientModeBrowserProxy();
    settings.AmbientModeBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();

    const prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    return CrSettingsPrefs.initialized.then(function() {
      ambientModePage = document.createElement('settings-ambient-mode-page');
      ambientModePage.prefs = prefElement.prefs;

      ambientModePage.prefs.settings.ambient_mode = {
        enabled: {value: true},
      };

      document.body.appendChild(ambientModePage);
      Polymer.dom.flush();
    });
  });

  teardown(function() {
    ambientModePage.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('toggleAmbientMode', function() {
    const button = ambientModePage.$$('#ambientModeEnable');
    assertTrue(!!button);
    assertFalse(button.disabled);

    // The button's state is set by the pref value.
    const enabled =
        ambientModePage.getPref('settings.ambient_mode.enabled.value');
    assertEquals(enabled, button.checked);

    // Click the button will toggle the pref value.
    button.click();
    Polymer.dom.flush();
    const enabled_toggled =
        ambientModePage.getPref('settings.ambient_mode.enabled.value');
    assertEquals(enabled_toggled, button.checked);
    assertEquals(enabled, !enabled_toggled);

    // Click again will toggle the pref value.
    button.click();
    Polymer.dom.flush();
    const enabled_toggled_twice =
        ambientModePage.getPref('settings.ambient_mode.enabled.value');
    assertEquals(enabled_toggled_twice, button.checked);
    assertEquals(enabled, enabled_toggled_twice);
  });

  test('doubleClickTopicSource', () => {
    // Select the google photos topic source.
    cr.webUIListenerCallback('topic-source-changed', {
      'topicSource': AmbientModeTopicSource.GOOGLE_PHOTOS,
      'hasAlbums': true
    });

    const topicSourceList = ambientModePage.$$('topic-source-list');
    const ironList = topicSourceList.$$('iron-list');
    const topicSourceItem =
        ironList.querySelector('topic-source-item[checked]');
    const clickableDiv = topicSourceItem.$$('#rowContainer');

    // Verify that the show-albums event is sent when the google photos radio
    // button is clicked again.
    let showAlbumEventCalls = 0;
    topicSourceList.addEventListener('show-albums', (event) => {
      assertEquals(AmbientModeTopicSource.GOOGLE_PHOTOS, event.detail);
      showAlbumEventCalls++;
    });

    clickableDiv.click();
    assertEquals(1, showAlbumEventCalls);

    // Should navigate to the ambient-mode/photos?topic-source=0 subpage.
    const router = settings.Router.getInstance();
    assertEquals('/ambientMode/photos', router.getCurrentRoute().path);
    assertEquals('topicSource=0', router.getQueryParameters().toString());
  });

  test('hasTopicSourceItems', function() {
    const topicSourceListElement = ambientModePage.$$('topic-source-list');
    const ironList = topicSourceListElement.$$('iron-list');
    const topicSourceItems = ironList.querySelectorAll('topic-source-item');
    assertEquals(2, topicSourceItems.length);
  });

  test('topicSourceItemHasCorrectRowHeight', function() {
    const topicSourceListElement = ambientModePage.$$('topic-source-list');
    const ironList = topicSourceListElement.$$('iron-list');
    const topicSourceItems = ironList.querySelectorAll('topic-source-item');

    topicSourceItems.forEach((row) => {
      assertEquals(64, row.offsetHeight);
    });
  });

  test('Deep link to topic sources', async () => {
    loadTimeData.overrideValues({isDeepLinkingEnabled: true});
    assertTrue(loadTimeData.getBoolean('isDeepLinkingEnabled'));

    const params = new URLSearchParams;
    params.append('settingId', '502');
    settings.Router.getInstance().navigateTo(
        settings.routes.AMBIENT_MODE, params);

    const deepLinkElement =
        ambientModePage.$$('topic-source-list').$$('topic-source-item');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Topic sources row should be focused for settingId=502.');
  });

  test('temperatureUnitRadioButtonsDisabled', () => {
    // When |selectedTemperatureUnit_| is invalid the radio buttons should be
    // disabled. This is the initial state.
    const radioGroup = ambientModePage.$$('#weatherDiv cr-radio-group');

    assertTrue(radioGroup.disabled);

    // When |selectedTemperatureUnit_| is valid the radio buttons should be
    // enabled.
    cr.webUIListenerCallback(
        'temperature-unit-changed', AmbientModeTemperatureUnit.CELSIUS);
    assertFalse(radioGroup.disabled);

    cr.webUIListenerCallback(
        'temperature-unit-changed', AmbientModeTemperatureUnit.UNKNOWN);
    assertTrue(radioGroup.disabled);
  });

  test('temperatureUnitRadioButtons', async () => {
    // Simulate C++ setting celsius as the initial temperature unit.
    cr.webUIListenerCallback(
        'temperature-unit-changed', AmbientModeTemperatureUnit.CELSIUS);

    const celsiusButton = ambientModePage.$$('cr-radio-button[name=celsius]');
    const fahrenheitButton =
        ambientModePage.$$('cr-radio-button[name=fahrenheit]');

    assertTrue(celsiusButton.checked);
    assertFalse(fahrenheitButton.checked);

    browserProxy.resetResolver('setSelectedTemperatureUnit');

    // Click fahrenheit and expect the fahrenheit radio button to be checked and
    // the browser proxy to be called with the correct argument.
    fahrenheitButton.click();

    assertFalse(celsiusButton.checked);
    assertTrue(fahrenheitButton.checked);

    assertEquals(1, browserProxy.getCallCount('setSelectedTemperatureUnit'));
    const fahrenheitArgs =
        await browserProxy.whenCalled('setSelectedTemperatureUnit');
    assertDeepEquals(['fahrenheit'], fahrenheitArgs);

    browserProxy.resetResolver('setSelectedTemperatureUnit');

    // Click celsius and expect the celsius radio button to be checked and the
    // browser proxy to be called with the correct argument.
    celsiusButton.click();

    assertTrue(celsiusButton.checked);
    assertFalse(fahrenheitButton.checked);

    assertEquals(1, browserProxy.getCallCount('setSelectedTemperatureUnit'));
    const celsiusArgs =
        await browserProxy.whenCalled('setSelectedTemperatureUnit');
    assertDeepEquals(['celsius'], celsiusArgs);
  });

  test('temperatureUnitRadioButtonsDoubleClick', async () => {
    // Simulate C++ setting celsius as the default temperature unit.
    cr.webUIListenerCallback(
        'temperature-unit-changed', AmbientModeTemperatureUnit.CELSIUS);

    const celsiusButton = ambientModePage.$$('cr-radio-button[name=celsius]');

    browserProxy.resetResolver('setSelectedTemperatureUnit');

    // Nothing should happen.
    celsiusButton.click();
    assertEquals(0, browserProxy.getCallCount('setSelectedTemperatureUnit'));
  });
});
