// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {DevicePageBrowserProxyImpl, Router, routes, setDisplayApiForTesting, setDisplaySettingsProviderForTesting, SettingsDisplayElement} from 'chrome://os-settings/os_settings.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush, microTask} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakeSystemDisplay} from '../fake_system_display.js';

import {getFakePrefs} from './device_page_test_util.js';
import {FakeDisplaySettingsProvider} from './fake_display_settings_provider.js';
import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

import DisplayUnitInfo = chrome.system.display.DisplayUnitInfo;

suite('<settings-display>', () => {
  let displayPage: SettingsDisplayElement;
  let fakeSystemDisplay: FakeSystemDisplay;
  let browserProxy: any;
  let displaySettingsProvider: FakeDisplaySettingsProvider;

  // Add a fake display.
  function addDisplay(displayIndex: number): void {
    const display = {
      id: 'fakeDisplayId' + displayIndex,
      name: 'fakeDisplayName' + displayIndex,
      mirroring: '',
      isPrimary: displayIndex === 1,
      isInternal: displayIndex === 1,
      rotation: 0,
      modes: [
        {
          deviceScaleFactor: 1.0,
          widthInNativePixels: 1920,
          heightInNativePixels: 1080,
          width: 1920,
          height: 1080,
          refreshRate: 60,
        },
        {
          deviceScaleFactor: 1.0,
          widthInNativePixels: 1920,
          heightInNativePixels: 1080,
          width: 1920,
          height: 1080,
          refreshRate: 30,
        },
        {
          deviceScaleFactor: 1.0,
          widthInNativePixels: 3000,
          heightInNativePixels: 2000,
          width: 3000,
          height: 2000,
          refreshRate: 45,
        },
        {
          deviceScaleFactor: 1.0,
          widthInNativePixels: 3000,
          heightInNativePixels: 2000,
          width: 3000,
          height: 2000,
          refreshRate: 75,
        },
        // Include 3 copies of 3000x2000 mode to emulate duplicated modes
        // reported by some monitors.  Only one is marked 'isNative'.
        {
          deviceScaleFactor: 1.0,
          widthInNativePixels: 3000,
          heightInNativePixels: 2000,
          width: 3000,
          height: 2000,
          refreshRate: 100,
        },
        {
          isNative: true,
          deviceScaleFactor: 1.0,
          widthInNativePixels: 3000,
          heightInNativePixels: 2000,
          width: 3000,
          height: 2000,
          refreshRate: 100,
        },
        {
          deviceScaleFactor: 1.0,
          widthInNativePixels: 3000,
          heightInNativePixels: 2000,
          width: 3000,
          height: 2000,
          refreshRate: 100,
        },
      ],
      bounds: {
        left: 0,
        top: 0,
        width: 1920,
        height: 1080,
      },
      availableDisplayZoomFactors: [1, 1.25, 1.5, 2],
    };
    fakeSystemDisplay.addDisplayForTest(display);
  }

  async function initPage() {
    displayPage = document.createElement('settings-display');
    displayPage.set('prefs', getFakePrefs());
    document.body.appendChild(displayPage);
    await flushTasks();
  }

  setup(async () => {
    Router.getInstance().navigateTo(routes.DISPLAY);

    fakeSystemDisplay = new FakeSystemDisplay();
    setDisplayApiForTesting(fakeSystemDisplay as any);

    DevicePageBrowserProxyImpl.setInstanceForTesting(
        new TestDevicePageBrowserProxy());
    browserProxy = DevicePageBrowserProxyImpl.getInstance();

    displaySettingsProvider = new FakeDisplaySettingsProvider();
    setDisplaySettingsProviderForTesting(displaySettingsProvider);
  });

  teardown(() => {
    displayPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('display tests', async function() {
    await initPage();

    // Verify all the conditionals that get run during page load
    // before the display info has been populated.
    assertEquals(undefined, displayPage.displays);
    assertFalse(displayPage.showMirror(true, displayPage.displays));
    assertFalse(displayPage.showMirror(false, displayPage.displays));
    assertFalse(displayPage.isMirrored(displayPage.displays));
    assertFalse(displayPage.showUnifiedDesktop(
        true, true, displayPage.displays, /*isTabletMode=*/ false));
    assertFalse(displayPage.showUnifiedDesktop(
        false, false, displayPage.displays, /*isTabletMode=*/ false));
    assertEquals(
        displayPage.getInvalidDisplayId(),
        browserProxy.lastHighlightedDisplayId);

    // Add a display.
    addDisplay(1);
    fakeSystemDisplay.onDisplayChanged.callListeners();

    return Promise
        .all([
          fakeSystemDisplay.getInfoCalled.promise,
          fakeSystemDisplay.getLayoutCalled.promise,
        ])
        .then(function() {
          // There should be a single display which should be primary and
          // selected. Mirroring should be disabled.
          assertEquals(1, displayPage.displays.length);
          assertEquals(
              displayPage.displays[0]!.id, displayPage.selectedDisplay!.id);
          assertEquals(
              displayPage.displays[0]!.id, displayPage.primaryDisplayId);
          assertFalse(displayPage.showMirror(false, displayPage.displays));
          assertFalse(displayPage.isMirrored(displayPage.displays));

          // Verify unified desktop only shown when enabled.
          assertTrue(displayPage.showUnifiedDesktop(
              true, true, displayPage.displays, /*isTabletMode=*/ false));
          assertFalse(displayPage.showUnifiedDesktop(
              false, false, displayPage.displays, /*isTabletMode=*/ false));

          // Sanity check the first display is internal.
          assertTrue(displayPage.displays[0]!.isInternal);

          // Ambient EQ only shown when enabled.
          assertTrue(displayPage.showAmbientColorSetting(
              true, displayPage.displays[0] as DisplayUnitInfo));
          assertFalse(displayPage.showAmbientColorSetting(
              false, displayPage.displays[0] as DisplayUnitInfo));

          // Verify that the arrangement section is not shown.
          assertEquals(
              null,
              displayPage.shadowRoot!.querySelector('#arrangement-section'));

          // Add a second display.
          addDisplay(2);
          fakeSystemDisplay.onDisplayChanged.callListeners();

          return Promise.all([
            fakeSystemDisplay.getInfoCalled.promise,
            fakeSystemDisplay.getLayoutCalled.promise,
            new Promise(function(resolve) {
              setTimeout(resolve);
            }),
          ]);
        })
        .then(function() {
          // There should be two displays, the first should be primary and
          // selected. Mirroring should be enabled but set to false.
          assertEquals(2, displayPage.displays.length);
          assertEquals(
              displayPage.displays[0]!.id, displayPage.selectedDisplay!.id);
          assertEquals(
              displayPage.displays[0]!.id, displayPage.primaryDisplayId);
          assertTrue(displayPage.showMirror(false, displayPage.displays));
          assertFalse(displayPage.isMirrored(displayPage.displays));

          // Verify unified desktop only shown when enabled.
          assertTrue(displayPage.showUnifiedDesktop(
              true, true, displayPage.displays, /*isTabletMode=*/ false));
          assertFalse(displayPage.showUnifiedDesktop(
              false, false, displayPage.displays, /*isTabletMode=*/ false));

          // Sanity check the second display is not internal.
          assertFalse(displayPage.displays[1]!.isInternal);


          // Verify the display modes are parsed correctly.

          // 5 total modes, 2 parent modes.
          assertEquals(7, displayPage.getModeToParentModeMap().size);
          assertEquals(0, displayPage.getModeToParentModeMap().get(0));
          assertEquals(0, displayPage.getModeToParentModeMap().get(1));
          assertEquals(5, displayPage.getModeToParentModeMap().get(2));
          assertEquals(5, displayPage.getModeToParentModeMap().get(3));
          assertEquals(5, displayPage.getModeToParentModeMap().get(4));
          assertEquals(5, displayPage.getModeToParentModeMap().get(5));
          assertEquals(5, displayPage.getModeToParentModeMap().get(6));

          // Two resolution options, one for each parent mode.
          assertEquals(2, displayPage.getRefreshRateList().length);

          // Each parent mode has the correct number of refresh rates.
          assertEquals(2, displayPage.getParentModeToRefreshRateMap().size);
          assertEquals(
              2, displayPage.getParentModeToRefreshRateMap().get(0)!.length);
          assertEquals(
              3, displayPage.getParentModeToRefreshRateMap().get(5)!.length);

          // Ambient EQ never shown on non-internal display regardless of
          // whether it is enabled.
          assertFalse(displayPage.showAmbientColorSetting(
              true, displayPage.displays[1] as DisplayUnitInfo));
          assertFalse(displayPage.showAmbientColorSetting(
              false, displayPage.displays[1] as DisplayUnitInfo));

          // Verify that the arrangement section is shown.
          assertTrue(
              !!displayPage.shadowRoot!.querySelector('#arrangement-section'));

          // Select the second display and make it primary. Also change the
          // orientation of the second display.
          const displayLayout =
              displayPage.shadowRoot!.querySelector('#displayLayout');
          assertTrue(!!displayLayout);
          const displayDiv = strictQuery(
              '#_fakeDisplayId2', displayLayout.shadowRoot, HTMLElement);
          assertTrue(!!displayDiv);
          displayDiv.click();
          assertEquals(
              displayPage.displays[1]!.id, displayPage.selectedDisplay!.id);
          flush();

          const primaryDisplaySelect = displayPage.shadowRoot!.getElementById(
                                           'primaryDisplaySelect') as any;
          assertTrue(!!primaryDisplaySelect);
          primaryDisplaySelect.value = '0';
          primaryDisplaySelect.dispatchEvent(new CustomEvent('change'));
          flush();

          const orientationSelect = displayPage.shadowRoot!.getElementById(
                                        'orientationSelect') as any;
          assertTrue(!!orientationSelect);
          orientationSelect.value = '90';
          orientationSelect.dispatchEvent(new CustomEvent('change'));
          flush();

          fakeSystemDisplay.onDisplayChanged.callListeners();

          return Promise.all([
            fakeSystemDisplay.getInfoCalled.promise,
            fakeSystemDisplay.getLayoutCalled.promise,
            new Promise(function(resolve) {
              setTimeout(resolve);
            }),
          ]);
        })
        .then(function() {
          // Confirm that the second display is selected, primary, and
          // rotated.
          assertEquals(2, displayPage.displays.length);
          assertEquals(
              displayPage.displays[1]!.id, displayPage.selectedDisplay!.id);
          assertTrue(displayPage.displays[1]!.isPrimary);
          assertEquals(
              displayPage.displays[1]!.id, displayPage.primaryDisplayId);
          assertEquals(90, displayPage.displays[1]!.rotation);

          // Mirror the displays.
          const displayMirrorCheckbox = strictQuery(
              '#displayMirrorCheckbox', displayPage.shadowRoot, HTMLElement);
          assertTrue(!!displayMirrorCheckbox);
          displayMirrorCheckbox.click();
          flush();

          fakeSystemDisplay.onDisplayChanged.callListeners();

          return Promise.all([
            fakeSystemDisplay.getInfoCalled.promise,
            fakeSystemDisplay.getLayoutCalled.promise,
            new Promise(function(resolve) {
              setTimeout(resolve);
            }),
          ]);
        })
        .then(function() {
          // Confirm that there is now only one display and that it is
          // primary and mirroring is enabled.
          assertEquals(1, displayPage.displays.length);
          assertEquals(
              displayPage.displays[0]!.id, displayPage.selectedDisplay!.id);
          assertTrue(displayPage.displays[0]!.isPrimary);
          assertTrue(displayPage.showMirror(false, displayPage.displays));
          assertTrue(displayPage.isMirrored(displayPage.displays));

          // Verify that the arrangement section is shown while mirroring.
          assertTrue(
              !!displayPage.shadowRoot!.querySelector('#arrangement-section'));

          // Ensure that the zoom value remains unchanged while draggging.
          function pointerEvent(eventType: string, ratio: number): void {
            const crSlider = displayPage.$.displaySizeSlider.$.slider;
            const rect = crSlider.$.container.getBoundingClientRect();
            crSlider.dispatchEvent(new PointerEvent(eventType, {
              buttons: 1,
              pointerId: 1,
              clientX: rect.left + (ratio * rect.width),
            }));
          }

          assertEquals(1, displayPage.getSelectedZoomPref().value);
          pointerEvent('pointerdown', .6);
          assertEquals(1, displayPage.getSelectedZoomPref().value);
          pointerEvent('pointermove', .3);
          assertEquals(1, displayPage.getSelectedZoomPref().value);
          pointerEvent('pointerup', 0);
          assertEquals(1.25, displayPage.getSelectedZoomPref().value);

          // Navigate out of the display page.
          Router.getInstance().navigateTo(routes.POWER);
        })
        .then(function() {
          // Moving out of the display page should set selected display to
          // invalid.
          assertEquals(
              displayPage.getInvalidDisplayId(),
              browserProxy.lastHighlightedDisplayId);

          // Navigate back to the display page.
          Router.getInstance().navigateTo(routes.DISPLAY);
        });
  });

  test('Deep link to display mirroring', async () => {
    await initPage();

    const params = new URLSearchParams();
    params.append('settingId', '428');
    Router.getInstance().navigateTo(routes.DISPLAY, params);

    addDisplay(1);
    addDisplay(2);
    fakeSystemDisplay.onDisplayChanged.callListeners();
    await fakeSystemDisplay.getInfoCalled.promise;
    await fakeSystemDisplay.getLayoutCalled.promise;
    assertEquals(2, displayPage.displays.length);

    flush();
    assert(displayPage);
    assertEquals(2, displayPage.displays.length);
    assertTrue(displayPage.shouldShowArrangementSection());

    const deepLinkElement =
        displayPage.shadowRoot!.querySelector('#displayMirrorCheckbox')!
            .shadowRoot!.querySelector('#checkbox');
    await waitAfterNextRender(deepLinkElement as HTMLElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Display mirroring checkbox should be focused for settingId=428.');
  });

  test('Keyboard display arrangement', async () => {
    await initPage();

    addDisplay(1);
    addDisplay(2);
    fakeSystemDisplay.onDisplayChanged.callListeners();

    return Promise
        .all([
          fakeSystemDisplay.getInfoCalled.promise,
          fakeSystemDisplay.getLayoutCalled.promise,
        ])
        .then(() => {
          return new Promise(resolve => {
            flush();

            assert(displayPage);
            assertEquals(2, displayPage.displays.length);
            assertTrue(displayPage.shouldShowArrangementSection());

            assertTrue(!!displayPage.shadowRoot!.querySelector(
                '#arrangement-section'));

            assertTrue(displayPage.showMirror(false, displayPage.displays));
            assertFalse(displayPage.isMirrored(displayPage.displays));

            flush();

            microTask.run(resolve);
          });
        })
        .then(() => {
          const displayLayout =
              displayPage.shadowRoot!.querySelector('#displayLayout') as any;
          assert(!!displayLayout);
          const display = strictQuery(
              '#_fakeDisplayId2', displayLayout.shadowRoot, HTMLElement);
          const layout = displayLayout.displayLayoutMap_.get('fakeDisplayId2');

          assertEquals(layout.parentId, 'fakeDisplayId1');
          assertEquals(layout.position, 'right');

          const offset =
              displayLayout.keyboardDragStepSize / displayLayout.visualScale;

          assert(!!display);
          display.focus();

          display.dispatchEvent(
              new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
          display.dispatchEvent(
              new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
          assertEquals(offset, layout.offset);

          display.dispatchEvent(
              new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
          display.dispatchEvent(
              new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
          assertEquals(offset * 2, layout.offset);

          display.dispatchEvent(
              new KeyboardEvent('keydown', {key: 'ArrowUp', bubbles: true}));
          display.dispatchEvent(
              new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
          assertEquals(offset, layout.offset);
        });
  });

  test('Unified desktop not supported in tablet mode', async () => {
    await initPage();

    addDisplay(1);
    addDisplay(2);
    fakeSystemDisplay.onDisplayChanged.callListeners();
    await fakeSystemDisplay.getInfoCalled.promise;
    await fakeSystemDisplay.getLayoutCalled.promise;
    assertEquals(2, displayPage.displays.length);

    // Unified desktop is supported when in clamshell mode.
    displaySettingsProvider.setTabletMode(false);
    assertTrue(displayPage.showUnifiedDesktop(
        /*unifiedDesktopAvailable=*/ true, /*unifiedDesktopMode=*/ false,
        displayPage.displays, displaySettingsProvider.getIsTabletMode()));

    // Unified desktop is not supported when in tablet mode.
    displaySettingsProvider.setTabletMode(true);
    assertFalse(displayPage.showUnifiedDesktop(
        /*unifiedDesktopAvailable=*/ true, /*unifiedDesktopMode=*/ false,
        displayPage.displays, displaySettingsProvider.getIsTabletMode()));
  });

  test('night light', async function() {
    await initPage();

    // Set up a single display.
    addDisplay(1);
    fakeSystemDisplay.onDisplayChanged.callListeners();
    await fakeSystemDisplay.getInfoCalled.promise;
    await fakeSystemDisplay.getLayoutCalled.promise;
    assertEquals(1, displayPage.displays.length);

    const displayNightLight = strictQuery(
        'settings-display-night-light', displayPage.shadowRoot, HTMLElement);
    assert(displayNightLight);

    const temperature = strictQuery(
        '#nightLightTemperatureDiv', displayNightLight.shadowRoot, HTMLElement);
    const schedule = strictQuery(
        '#nightLightScheduleTypeDropDown', displayNightLight.shadowRoot,
        HTMLElement);

    // Night Light is off, so temperature is hidden. Schedule is always shown.
    assertTrue(temperature.hidden);
    assertFalse(schedule.hidden);

    // // Enable Night Light. Use an atomic update of |displayPage.prefs| so
    // // Polymer notices the change.
    const newPrefs = getFakePrefs();
    newPrefs.ash.night_light.enabled.value = true;
    displayPage.prefs = newPrefs;
    flush();

    // // Night Light is on, so temperature is visible.
    assertFalse(temperature.hidden);
    assertFalse(schedule.hidden);
  });
});
