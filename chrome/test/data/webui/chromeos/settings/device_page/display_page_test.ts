// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {CrA11yAnnouncerElement, CrLinkRowElement, CrSliderElement, CrToggleElement, DevicePageBrowserProxyImpl, DisplayLayoutElement, displaySettingsProviderMojom, GeolocationAccessLevel, NightLightScheduleType, Router, routes, setDisplayApiForTesting, setDisplaySettingsProviderForTesting, SettingsDisplayElement, SettingsDropdownMenuElement, SettingsSliderElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush, microTask} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {FakeSystemDisplay} from '../fake_system_display.js';

import {getFakePrefs, pressArrowLeft, pressArrowRight, simulateSliderClicked} from './device_page_test_util.js';
import {FakeDisplaySettingsProvider} from './fake_display_settings_provider.js';
import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

import DisplayUnitInfo = chrome.system.display.DisplayUnitInfo;

const kDisplayIdPrefix = '123456789';

suite('<settings-display>', () => {
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');
  let displayPage: SettingsDisplayElement;
  let fakeSystemDisplay: FakeSystemDisplay;
  let browserProxy: any;
  let displaySettingsProvider: FakeDisplaySettingsProvider;

  // Add a fake display.
  function addDisplay(displayIndex: number): void {
    const display = {
      id: kDisplayIdPrefix + displayIndex,
      name: 'fakeDisplayName' + displayIndex,
      mirroring: '',
      isPrimary: displayIndex === 1,
      isInternal: displayIndex === 1,
      rotation: 0,
      modes: [
        {
          isNative: true,
          deviceScaleFactor: 1.0,
          widthInNativePixels: 1920,
          heightInNativePixels: 1080,
          width: 1920,
          height: 1080,
          refreshRate: 60,
          isSelected: true,
        },
        {
          isNative: true,
          deviceScaleFactor: 1.0,
          widthInNativePixels: 1920,
          heightInNativePixels: 1080,
          width: 1920,
          height: 1080,
          refreshRate: 30,
          isSelected: true,
        },
        {
          isNative: true,
          deviceScaleFactor: 1.0,
          widthInNativePixels: 3000,
          heightInNativePixels: 2000,
          width: 3000,
          height: 2000,
          refreshRate: 45,
          isSelected: true,
        },
        {
          isNative: true,
          deviceScaleFactor: 1.0,
          widthInNativePixels: 3000,
          heightInNativePixels: 2000,
          width: 3000,
          height: 2000,
          refreshRate: 75,
          isSelected: true,
        },
        // Include 3 copies of 3000x2000 mode to emulate duplicated modes
        // reported by some monitors.  Only one is marked 'isNative'.
        {
          isNative: false,
          deviceScaleFactor: 1.0,
          widthInNativePixels: 3000,
          heightInNativePixels: 2000,
          width: 3000,
          height: 2000,
          refreshRate: 100,
          isSelected: true,
        },
        {
          isNative: true,
          deviceScaleFactor: 1.0,
          widthInNativePixels: 3000,
          heightInNativePixels: 2000,
          width: 3000,
          height: 2000,
          refreshRate: 100,
          isSelected: true,
        },
        {
          isNative: false,
          deviceScaleFactor: 1.0,
          widthInNativePixels: 3000,
          heightInNativePixels: 2000,
          width: 3000,
          height: 2000,
          refreshRate: 100,
          isSelected: true,
        },
      ],
      bounds: {
        left: 0,
        top: 0,
        width: 1920,
        height: 1080,
      },
      availableDisplayZoomFactors: [1, 1.25, 1.5, 2],

      // The properties below were added solely for compilation purposes. Values
      // may not reflect valid or real states.
      mirroringSourceId: '',
      mirroringDestinationIds: [],
      activeState: chrome.system.display.ActiveState.ACTIVE,
      isEnabled: true,
      isUnified: false,
      dpiX: 0,
      dpiY: 0,
      overscan: {
        left: 0,
        top: 0,
        right: 0,
        bottom: 0,
      },
      workArea: {
        left: 0,
        top: 0,
        width: 0,
        height: 0,
      },
      hasTouchSupport: false,
      hasAccelerometerSupport: false,
      displayZoomFactor: 1,
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
    setDisplayApiForTesting(fakeSystemDisplay);

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

  suite('Display settings histogram tests', () => {
    let displayHistogram:
        Map<displaySettingsProviderMojom.DisplaySettingsType, number>;
    let externalDisplayHistogram:
        Map<displaySettingsProviderMojom.DisplaySettingsType, number>;

    // Helper function for testing histogram metrics.
    async function initHistogramTest(): Promise<void> {
      await initPage();

      // Add a display.
      addDisplay(1);
      fakeSystemDisplay.onDisplayChanged.callListeners();
      await fakeSystemDisplay.getInfoCalled.promise;
      await fakeSystemDisplay.getLayoutCalled.promise;

      // Sanity check display count and the first display is an internal
      // display.
      assertEquals(1, displayPage.displays.length);
      assertTrue(displayPage.displays[0]!.isInternal);

      // Add a second display.
      addDisplay(2);
      fakeSystemDisplay.onDisplayChanged.callListeners();
      await fakeSystemDisplay.getInfoCalled.promise;
      await fakeSystemDisplay.getLayoutCalled.promise;
      flush();

      // Sanity check display count and the second display is an external
      // display.
      assertEquals(2, displayPage.displays.length);
      assertFalse(displayPage.displays[1]!.isInternal);

      // Select the second display.
      const displayLayout =
          displayPage.shadowRoot!.querySelector('#displayLayout');
      assertTrue(!!displayLayout);
      const displayDiv = strictQuery(
          `#_${kDisplayIdPrefix}2`, displayLayout.shadowRoot, HTMLElement);
      assertTrue(!!displayDiv);
      displayDiv.click();

      // Sanity check the second display is selected.
      assertEquals(
          displayPage.displays[1]!.id, displayPage.selectedDisplay!.id);
      flush();
    }

    setup(async () => {
      loadTimeData.overrideValues({unifiedDesktopAvailable: true});

      await initHistogramTest();
      displayHistogram = displaySettingsProvider.getDisplayHistogram();
      externalDisplayHistogram =
          displaySettingsProvider.getExternalDisplayHistogram();
    });

    test('page load', async () => {
      // Verify histogram count for display settings page opened.
      assertEquals(
          1,
          displayHistogram.get(
              displaySettingsProviderMojom.DisplaySettingsType.kDisplayPage));
    });

    test('resolution', async () => {
      // Mock user changing display resolution.
      const displayModeSelector =
          displayPage.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
              '#displayModeSelector');
      assertTrue(!!displayModeSelector);
      displayModeSelector.pref = {
        key: 'prefs.cros.device_display_resolution',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: 5,
      };
      displayModeSelector.dispatchEvent(new CustomEvent('change'));

      fakeSystemDisplay.onDisplayChanged.callListeners();
      await fakeSystemDisplay.getInfoCalled.promise;
      await fakeSystemDisplay.getLayoutCalled.promise;
      flush();

      // Verify histogram count for resolution change.
      assertEquals(
          1,
          externalDisplayHistogram.get(
              displaySettingsProviderMojom.DisplaySettingsType.kResolution));
    });

    test('refresh rate', async () => {
      // Mock user changing refresh rate.
      const refreshRateSelector =
          displayPage.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
              '#refreshRateSelector');
      assertTrue(!!refreshRateSelector);
      refreshRateSelector.pref = {
        key: 'prefs.cros.device_display_resolution',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: 1,
      };
      refreshRateSelector.dispatchEvent(new CustomEvent('change'));

      fakeSystemDisplay.onDisplayChanged.callListeners();
      await fakeSystemDisplay.getInfoCalled.promise;
      await fakeSystemDisplay.getLayoutCalled.promise;
      flush();

      // Verify histogram count for refresh rate change.
      assertEquals(
          1,
          externalDisplayHistogram.get(
              displaySettingsProviderMojom.DisplaySettingsType.kRefreshRate));
    });

    test('scaling', async () => {
      // Mock user changing scaling.
      const displaySizeSlider =
          displayPage.shadowRoot!.querySelector<SettingsSliderElement>(
              '#displaySizeSlider');
      assertTrue(!!displaySizeSlider);
      displaySizeSlider.pref = {
        key: 'prefs.cros.device_display_resolution',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: 1.2,
      };
      displaySizeSlider.dispatchEvent(new CustomEvent('change'));

      fakeSystemDisplay.onDisplayChanged.callListeners();
      await fakeSystemDisplay.getInfoCalled.promise;
      await fakeSystemDisplay.getLayoutCalled.promise;
      flush();

      // Verify histogram count for scaling change.
      assertEquals(
          1,
          externalDisplayHistogram.get(
              displaySettingsProviderMojom.DisplaySettingsType.kScaling));
    });

    test('orientation', async () => {
      // Mock user changing orientation.
      const orientationSelect =
          displayPage.shadowRoot!.querySelector<HTMLSelectElement>(
              '#orientationSelect');
      assertTrue(!!orientationSelect);
      orientationSelect.value = '90';
      orientationSelect.dispatchEvent(new CustomEvent('change'));

      fakeSystemDisplay.onDisplayChanged.callListeners();
      await fakeSystemDisplay.getInfoCalled.promise;
      await fakeSystemDisplay.getLayoutCalled.promise;
      flush();

      // Verify histogram count for orientation change.
      const externalDisplayHistogram =
          displaySettingsProvider.getExternalDisplayHistogram();
      assertEquals(
          1,
          externalDisplayHistogram.get(
              displaySettingsProviderMojom.DisplaySettingsType.kOrientation));

      const externalDisplayOrientationHistogram =
          displaySettingsProvider.getDisplayOrientationHistogram(
              /*is_internal=*/ false);
      assertEquals(
          1,
          externalDisplayOrientationHistogram.get(
              displaySettingsProviderMojom.DisplaySettingsOrientationOption
                  .k90Degree));
    });

    test('overscan', () => {
      // Mock user opening overscan dialog.
      const displayOverscan =
          displayPage.shadowRoot!.querySelector<CrLinkRowElement>('#overscan');
      assertTrue(!!displayOverscan);
      displayOverscan.click();
      flush();

      // Verify histogram count for overscan setting.
      assertEquals(
          1,
          externalDisplayHistogram.get(
              displaySettingsProviderMojom.DisplaySettingsType.kOverscan));
    });

    test('night light', () => {
      // Mock user toggling night light button.
      const displayNightLight = strictQuery(
          'settings-display-night-light', displayPage.shadowRoot, HTMLElement);
      assertTrue(!!displayNightLight);
      const nightLightToggleButton =
          displayNightLight.shadowRoot!.getElementById(
              'nightLightToggleButton') as SettingsToggleButtonElement;
      assertTrue(!!nightLightToggleButton);
      nightLightToggleButton.click();
      flush();

      // Verify histogram count for night light setting.
      assertEquals(
          1,
          externalDisplayHistogram.get(
              displaySettingsProviderMojom.DisplaySettingsType.kNightLight));

      const externalDisplayNightLightStatusHistogram =
          displaySettingsProvider.getDisplayNightLightStatusHistogram(
              /*is_internal=*/ false);
      assertEquals(
          1,
          externalDisplayNightLightStatusHistogram.get(
              /*night_light_status=*/ true));

      // Mock user updating night light schedule.
      const schedule = displayNightLight.shadowRoot!
                           .querySelector<SettingsDropdownMenuElement>(
                               '#nightLightScheduleTypeDropDown');
      assertTrue(!!schedule);
      schedule.pref = {
        key: 'ash.night_light.schedule_type',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: 1,
      };

      // Verify histogram count for night light setting.
      assertEquals(
          1,
          externalDisplayHistogram.get(
              displaySettingsProviderMojom.DisplaySettingsType
                  .kNightLightSchedule));

      const externalDisplayNightLightScheduleHistogram =
          displaySettingsProvider.getDisplayNightLightScheduleHistogram(
              /*is_internal=*/ false);
      assertEquals(
          1,
          externalDisplayNightLightScheduleHistogram.get(
              displaySettingsProviderMojom
                  .DisplaySettingsNightLightScheduleOption.kSunsetToSunrise));
    });

    test('mirror mode', () => {
      // Mock user toggling mirror mode setting.
      const mirrorDisplayControl =
          displayPage.shadowRoot!.querySelector<HTMLElement>(
              isRevampWayfindingEnabled ? '#mirrorDisplayToggle' :
                                          '#displayMirrorCheckbox');
      assertTrue(!!mirrorDisplayControl);
      mirrorDisplayControl.click();

      // Verify histogram count for mirror mode setting.
      assertEquals(
          1,
          displayHistogram.get(
              displaySettingsProviderMojom.DisplaySettingsType.kMirrorMode));
      assertEquals(
          1,
          displaySettingsProvider.getDisplayMirrorModeStatusHistogram().get(
              /*mirror_mode_status=*/ true));

      // When revamp wayfinding is enabled, clicking on the row should toggle
      // mirror mode, too.
      if (isRevampWayfindingEnabled) {
        const mirrorDisplayRow =
            displayPage.shadowRoot!.querySelector<HTMLElement>(
                '#mirrorDisplayToggleButton');
        assertTrue(!!mirrorDisplayRow);
        mirrorDisplayRow.click();

        // Verify histogram count for mirror mode setting.
        assertEquals(
            2,
            displayHistogram.get(
                displaySettingsProviderMojom.DisplaySettingsType.kMirrorMode));
        assertEquals(
            1,
            displaySettingsProvider.getDisplayMirrorModeStatusHistogram().get(
                /*mirror_mode_status=*/ false));
      }
    });

    test('mirror mode with keyboard', () => {
      // Mock user toggling mirror mode setting with keyboard.
      const mirrorDisplayControl =
          displayPage.shadowRoot!.querySelector<HTMLElement>(
              isRevampWayfindingEnabled ? '#mirrorDisplayToggle' :
                                          '#displayMirrorCheckbox');
      assertTrue(!!mirrorDisplayControl);

      mirrorDisplayControl.focus();
      mirrorDisplayControl.dispatchEvent(new Event('change', {bubbles: true}));

      // Verify histogram count for mirror mode setting.
      assertEquals(
          1,
          displayHistogram.get(
              displaySettingsProviderMojom.DisplaySettingsType.kMirrorMode));
      assertEquals(
          1,
          displaySettingsProvider.getDisplayMirrorModeStatusHistogram().get(
              /*mirror_mode_status=*/ true));
    });

    test('unified mode', () => {
      // Mock user toggling unified mode setting.
      const displayUnifiedDesktopToggle =
          displayPage.shadowRoot!.querySelector<CrToggleElement>(
              '#displayUnifiedDesktopToggle');
      assertTrue(!!displayUnifiedDesktopToggle);
      displayUnifiedDesktopToggle.click();

      // Verify histogram count for unified mode setting.
      assertEquals(
          1,
          displayHistogram.get(
              displaySettingsProviderMojom.DisplaySettingsType.kUnifiedMode));
      assertEquals(
          1,
          displaySettingsProvider.getDisplayUnifiedModeStatusHistogram().get(
              /*mirror_mode_status=*/ true));
    });

    test('primary display', () => {
      // Mock user changing primary display.
      const primaryDisplaySelect =
          displayPage.shadowRoot!.querySelector<HTMLSelectElement>(
              '#primaryDisplaySelect');
      assertTrue(!!primaryDisplaySelect);
      primaryDisplaySelect.value = '1';
      primaryDisplaySelect.dispatchEvent(new CustomEvent('change'));
      flush();

      // Verify histogram count for changing primary display setting.
      assertEquals(
          1,
          displayHistogram.get(displaySettingsProviderMojom.DisplaySettingsType
                                   .kPrimaryDisplay));
    });
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
              `#_${kDisplayIdPrefix}2`, displayLayout.shadowRoot, HTMLElement);
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
          const mirrorDisplayControl = strictQuery(
              isRevampWayfindingEnabled ? '#mirrorDisplayToggle' :
                                          '#displayMirrorCheckbox',
              displayPage.shadowRoot, HTMLElement);
          assertTrue(!!mirrorDisplayControl);
          mirrorDisplayControl.click();
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

    const deepLinkElement = displayPage.shadowRoot!.querySelector<HTMLElement>(
        isRevampWayfindingEnabled ? '#mirrorDisplayToggle' :
                                    '#displayMirrorCheckbox');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, displayPage.shadowRoot!.activeElement,
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
        .then(async () => {
          const displayLayout =
              displayPage.shadowRoot!.querySelector<DisplayLayoutElement>(
                  '#displayLayout');
          assert(!!displayLayout);
          const display = strictQuery(
              `#_${kDisplayIdPrefix}2`, displayLayout.shadowRoot, HTMLElement);
          const layout = displayLayout.getDisplayLayoutMapForTesting().get(
              `${kDisplayIdPrefix}2`);
          assert(layout);

          assertEquals(layout.parentId, `${kDisplayIdPrefix}1`);
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

          const displayArea = strictQuery(
              '#displayArea', displayLayout.shadowRoot, HTMLDivElement);
          const announcer = strictQuery(
              'cr-a11y-announcer', displayArea, CrA11yAnnouncerElement);
          const messagesDiv =
              strictQuery('#messages', announcer.shadowRoot, HTMLDivElement);
          assert(!!messagesDiv);
          const announcementTimeout = 150;
          await new Promise(
              resolve => setTimeout(resolve, announcementTimeout));
          assertStringContains(
              messagesDiv.textContent!, 'Window moved downwards');

          display.dispatchEvent(
              new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
          display.dispatchEvent(
              new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
          assertEquals(offset * 2, layout.offset);
          await new Promise(
              resolve => setTimeout(resolve, announcementTimeout));
          assertStringContains(
              messagesDiv.textContent!, 'Window moved downwards');

          display.dispatchEvent(
              new KeyboardEvent('keydown', {key: 'ArrowUp', bubbles: true}));
          display.dispatchEvent(
              new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
          assertEquals(offset, layout.offset);
          await new Promise(
              resolve => setTimeout(resolve, announcementTimeout));
          assertStringContains(messagesDiv.textContent!, 'Window moved upwards');
        });
  });

  test('Exclude display visibility without flag/pref and with pref', async () => {
    await initPage();

    addDisplay(1);
    addDisplay(2);
    addDisplay(3);
    fakeSystemDisplay.onDisplayChanged.callListeners();
    await fakeSystemDisplay.getInfoCalled.promise;
    await fakeSystemDisplay.getLayoutCalled.promise;
    assertEquals(3, displayPage.displays.length);

    // Exclude Display is not supported without flag or pref.
    let excludeDisplayToggleRow =
        displayPage.shadowRoot!.querySelector('#excludeDisplayToggleRow');
    assertFalse(isVisible(excludeDisplayToggleRow));

    // Set pref to true.
    const newPrefs = getFakePrefs();
    newPrefs.settings.display.allow_exclude_display_in_mirror_mode.value = true;
    displayPage.prefs = newPrefs;
    flush();

    // Should now be visible.
    excludeDisplayToggleRow =
        displayPage.shadowRoot!.querySelector('#excludeDisplayToggleRow');
    assertTrue(isVisible(excludeDisplayToggleRow));
    // Visibility with flag will be tested with the feature.
  });

  test('Exclude display support with flag', async () => {
    loadTimeData.overrideValues({excludeDisplayInMirrorModeEnabled: true});
    await initPage();

    addDisplay(1);
    addDisplay(2);
    addDisplay(3);
    fakeSystemDisplay.onDisplayChanged.callListeners();
    await Promise.all([
      fakeSystemDisplay.getInfoCalled.promise,
      fakeSystemDisplay.getLayoutCalled.promise,
    ]);
    assertEquals(3, displayPage.displays.length);
    assertEquals(0, displayPage.mirroringDestinationIds.length);

    // Check the Exclude Display toggle is visible with flag set.
    const excludeDisplayToggleRow =
        displayPage.shadowRoot!.querySelector('#excludeDisplayToggleRow');
    assertTrue(isVisible(excludeDisplayToggleRow));

    // Exclude the current selected display.
    const excludeDisplayToggle =
        displayPage.shadowRoot!.querySelector<CrToggleElement>(
            '#excludeDisplayToggle');
    assertTrue(!!excludeDisplayToggle);
    excludeDisplayToggle.click();
    flush();

    assertTrue(!!excludeDisplayToggle);
    assertTrue(excludeDisplayToggle.checked);

    // Sanity check that we are not in mirror mode.
    assertTrue(displayPage.showMirror(false, displayPage.displays));
    assertFalse(displayPage.isMirrored(displayPage.displays));
    // Mirror the displays.
    const mirrorDisplayControl = strictQuery(
        isRevampWayfindingEnabled ? '#mirrorDisplayToggle' :
                                    '#displayMirrorCheckbox',
        displayPage.shadowRoot, HTMLElement);
    assertTrue(!!mirrorDisplayControl);
    mirrorDisplayControl.click();
    flush();

    fakeSystemDisplay.onDisplayChanged.callListeners();
    await Promise.all([
      fakeSystemDisplay.getInfoCalled.promise,
      fakeSystemDisplay.getLayoutCalled.promise,
      new Promise(function(resolve) {
        setTimeout(resolve);
      }),
    ]);

    assertTrue(displayPage.isMirrored(displayPage.displays));
    // There should be 2 displays in the list.
    assertEquals(2, displayPage.displays.length);
    // There should only be 1 display in mirroring
    // destination.
    assertEquals(1, displayPage.mirroringDestinationIds.length);
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

  test('night light displays geolocation warning', async () => {
    await initPage();

    // Check consumer flow.
    let newPrefs = getFakePrefs();
    newPrefs.ash.user.geolocation_access_level.value =
        GeolocationAccessLevel.DISALLOWED;
    newPrefs.ash.night_light.schedule_type.value =
        NightLightScheduleType.SUNSET_TO_SUNRISE;
    displayPage.prefs = newPrefs;
    flush();

    let displayNightLight = strictQuery(
        'settings-display-night-light', displayPage.shadowRoot, HTMLElement);
    assert(displayNightLight);

    let warningText = strictQuery(
        'settings-privacy-hub-geolocation-warning-text',
        displayNightLight.shadowRoot, HTMLElement);
    assert(warningText);
    assertTrue(warningText.getAttribute('warning-text-with-anchor')!.includes(
        '<a href="#">'));

    // Check managed flow, when Geolocation pref is enforced.
    newPrefs = getFakePrefs();
    newPrefs.ash.user.geolocation_access_level.value =
        GeolocationAccessLevel.DISALLOWED;
    newPrefs.ash.user.geolocation_access_level.enforcement =
        chrome.settingsPrivate.Enforcement.ENFORCED;
    newPrefs.ash.night_light.schedule_type.value =
        NightLightScheduleType.SUNSET_TO_SUNRISE;
    displayPage.prefs = newPrefs;
    flush();

    displayNightLight = strictQuery(
        'settings-display-night-light', displayPage.shadowRoot, HTMLElement);
    assert(displayNightLight);

    warningText = strictQuery(
        'settings-privacy-hub-geolocation-warning-text',
        displayNightLight.shadowRoot, HTMLElement);
    assert(warningText);
    assertFalse(warningText.getAttribute('warning-text-with-anchor')!.includes(
        '<a href="#">'));
  });

  test('Display Performance', async () => {
    await initPage();

    // Set up a single display.
    addDisplay(1);
    fakeSystemDisplay.onDisplayChanged.callListeners();
    await fakeSystemDisplay.getInfoCalled.promise;
    await fakeSystemDisplay.getLayoutCalled.promise;
    assertEquals(1, displayPage.displays.length);

    const displayPerformanceToggle = strictQuery(
        '#displayPerformanceModeToggle', displayPage.shadowRoot, HTMLElement);
    assertTrue(!!displayPerformanceToggle);
    displayPerformanceToggle.click();

    assertTrue(displaySettingsProvider.getShinyPerformance());
  });

  test('Display brightness, flag disabled', async () => {
    loadTimeData.overrideValues(
        {enableDisplayBrightnessControlInSettings: false});
    await initPage();

    // Set up a single display.
    addDisplay(1);
    fakeSystemDisplay.onDisplayChanged.callListeners();
    await fakeSystemDisplay.getInfoCalled.promise;
    await fakeSystemDisplay.getLayoutCalled.promise;
    assertEquals(1, displayPage.displays.length);

    // Brightness slider should not be present when the flag is disabled.
    const displayBrightnessWrapper =
        displayPage.shadowRoot!.querySelector<HTMLDivElement>(
            '#brightnessSliderWrapper');
    assertFalse(!!displayBrightnessWrapper);

    // Auto-brightness toggle should not be present when the flag is disabled.
    const displayAutoBrightnessToggle =
        displayPage.shadowRoot!.querySelector<CrToggleElement>(
            '#autoBrightnessToggle');
    assertFalse(!!displayAutoBrightnessToggle);
  });

  test('Display brightness, flag enabled on internal display', async () => {
    loadTimeData.overrideValues(
        {enableDisplayBrightnessControlInSettings: true});
    await initPage();

    // Set up the internal display.
    addDisplay(1);
    fakeSystemDisplay.onDisplayChanged.callListeners();
    await fakeSystemDisplay.getInfoCalled.promise;
    await fakeSystemDisplay.getLayoutCalled.promise;
    assertEquals(1, displayPage.displays.length);
    flush();

    // Display brightness slider should be present on the internal display when
    // the flag is enabled.
    const displayBrightnessWrapper =
        displayPage.shadowRoot!.querySelector<HTMLDivElement>(
            '#brightnessSliderWrapper');
    assertTrue(!!displayBrightnessWrapper);

    // Auto-brightness toggle should be present on the internal display when the
    // flag is enabled.
    const displayAutoBrightnessToggle =
        displayPage.shadowRoot!.querySelector<CrToggleElement>(
            '#autoBrightnessToggle');
    assertTrue(!!displayAutoBrightnessToggle);
  });

  test('Display brightness, flag enabled on external display', async () => {
    loadTimeData.overrideValues(
        {enableDisplayBrightnessControlInSettings: true});
    await initPage();

    // Set up the internal display.
    addDisplay(1);
    fakeSystemDisplay.onDisplayChanged.callListeners();
    await fakeSystemDisplay.getInfoCalled.promise;
    await fakeSystemDisplay.getLayoutCalled.promise;
    assertEquals(1, displayPage.displays.length);
    flush();

    // Set up an external display.
    addDisplay(2);
    fakeSystemDisplay.onDisplayChanged.callListeners();
    await fakeSystemDisplay.getInfoCalled.promise;
    await fakeSystemDisplay.getLayoutCalled.promise;
    assertEquals(2, displayPage.displays.length);
    flush();

    // Select the second display.
    const displayLayout =
        displayPage.shadowRoot!.querySelector('#displayLayout');
    assertTrue(!!displayLayout);
    const displayDiv = strictQuery(
        `#_${kDisplayIdPrefix}2`, displayLayout.shadowRoot, HTMLElement);
    assertTrue(!!displayDiv);
    displayDiv.click();

    // Check that the second display is selected.
    assertEquals(displayPage.displays[1]!.id, displayPage.selectedDisplay!.id);
    flush();

    const displayBrightness =
        displayPage.shadowRoot!.querySelector<HTMLDivElement>(
            '#brightnessSliderWrapper');

    // Display brightness slider should not be present on external displays.
    assertFalse(!!displayBrightness);

    // Auto-brightness toggle should not be present on external displays.
    const displayAutoBrightnessToggle =
        displayPage.shadowRoot!.querySelector<CrToggleElement>(
            '#autoBrightnessToggle');
    assertFalse(!!displayAutoBrightnessToggle);
  });

  test(
      'Display brightness, slider updates when brightness changes',
      async () => {
        loadTimeData.overrideValues(
            {enableDisplayBrightnessControlInSettings: true});
        await initPage();

        // Set up the internal display.
        addDisplay(1);
        fakeSystemDisplay.onDisplayChanged.callListeners();
        await fakeSystemDisplay.getInfoCalled.promise;
        await fakeSystemDisplay.getLayoutCalled.promise;
        assertEquals(1, displayPage.displays.length);
        flush();

        // Display brightness slider should be present on the internal display.
        const displayBrightness =
            displayPage.shadowRoot!.querySelector<HTMLDivElement>(
                '#brightnessSliderWrapper');
        assertTrue(!!displayBrightness);

        const initialBrightness = 22.2;
        displaySettingsProvider.setBrightnessPercentForTesting(
            initialBrightness, /*triggeredByAls=*/ false);
        await flushTasks();

        // Before changing the screen brightness, the slider value should be
        // equal to the current screen brightness.
        const displayBrightnessSlider =
            displayPage.shadowRoot!.querySelector<CrSliderElement>(
                '#brightnessSlider');
        assertTrue(!!displayBrightnessSlider);
        assertEquals(displayBrightnessSlider.value, initialBrightness);

        // Change the screen brightness.
        let adjustedBrightness = 99.0;
        displaySettingsProvider.setBrightnessPercentForTesting(
            adjustedBrightness, /*triggeredByAls=*/ false);
        await flushTasks();

        // The slider should update to the new brightness.
        assertEquals(displayBrightnessSlider.value, adjustedBrightness);

        // Change the screen brightness again.
        adjustedBrightness = 5.5;
        displaySettingsProvider.setBrightnessPercentForTesting(
            adjustedBrightness, /*triggeredByAls=*/ false);
        await flushTasks();

        // The slider should update to the new brightness.
        assertEquals(displayBrightnessSlider.value, adjustedBrightness);

        // Change the brightness to 5.5 again, but this time, it is triggered by
        // ambient light sensor.
        const minVisiblePercent = 10;
        adjustedBrightness = 5.5;
        displaySettingsProvider.setBrightnessPercentForTesting(
            adjustedBrightness, /*triggeredByAls=*/ true);
        await flushTasks();

        // The slider should update to the minVisiblePercent
        assertEquals(displayBrightnessSlider.value, minVisiblePercent);
      });

  test(
      'Display brightness set pref value from slider, flag enabled',
      async () => {
        loadTimeData.overrideValues(
            {enableDisplayBrightnessControlInSettings: true});
        await initPage();

        // Set up the internal display.
        addDisplay(1);
        fakeSystemDisplay.onDisplayChanged.callListeners();
        await fakeSystemDisplay.getInfoCalled.promise;
        await fakeSystemDisplay.getLayoutCalled.promise;
        assertEquals(1, displayPage.displays.length);
        flush();

        // Before adjusting the slider, the value in FakeDisplaySettingsProvider
        // should be equal to the default.
        assertEquals(
            displaySettingsProvider.getInternalDisplayScreenBrightness(), 0);

        const displayBrightnessSlider =
            displayPage.shadowRoot!.querySelector<CrSliderElement>(
                '#brightnessSlider');
        assertTrue(!!displayBrightnessSlider);

        // Test clicking to near-min brightness case.
        const minimumBrightnessPercent = 5;
        const nearMinimumPercent = minimumBrightnessPercent + 1;
        await simulateSliderClicked(
            displayBrightnessSlider, nearMinimumPercent,
            /*minimumValue=*/ minimumBrightnessPercent);
        // Round to nearest integer for comparison to avoid precision issues
        // (e.g. 6.000001) that don't affect real-world behavior.
        let roundedBrightness = Math.round(
            displaySettingsProvider.getInternalDisplayScreenBrightness());
        assertEquals(nearMinimumPercent, roundedBrightness);

        // Test clicking to max brightness case.
        const maxOutputBrightnessPercent = 100;
        await simulateSliderClicked(
            displayBrightnessSlider, maxOutputBrightnessPercent,
            /*minimumValue=*/ minimumBrightnessPercent);
        assertEquals(
            maxOutputBrightnessPercent,
            displaySettingsProvider.getInternalDisplayScreenBrightness(),
        );

        // Test clicking to non-boundary brightness case.
        const nonBoundaryOutputBrightnessPercent = 31;
        await simulateSliderClicked(
            displayBrightnessSlider, nonBoundaryOutputBrightnessPercent,
            /*minimumValue=*/ minimumBrightnessPercent);
        // Round to nearest integer for comparison to avoid precision issues
        // (e.g. 30.9999) that don't affect real-world behavior.
        roundedBrightness = Math.round(
            displaySettingsProvider.getInternalDisplayScreenBrightness());
        assertEquals(nonBoundaryOutputBrightnessPercent, roundedBrightness);

        // Ensure value clamps to min.
        displayBrightnessSlider.value = 0;
        await flushTasks();
        const sliderValueChangedPromise =
            eventToPromise('cr-slider-value-changed', displayBrightnessSlider);
        displayBrightnessSlider.dispatchEvent(
            new CustomEvent('cr-slider-value-changed'));
        await sliderValueChangedPromise;
        assertEquals(
            minimumBrightnessPercent,
            displaySettingsProvider.getInternalDisplayScreenBrightness());

        // Ensure value clamps to max.
        displayBrightnessSlider.value = 101;
        await flushTasks();
        displayBrightnessSlider.dispatchEvent(
            new CustomEvent('cr-slider-value-changed'));
        await sliderValueChangedPromise;
        assertEquals(
            maxOutputBrightnessPercent,
            displaySettingsProvider.getInternalDisplayScreenBrightness());

        // Set the value to somewhere in the middle.
        const expectedBrightnessValue = 77;
        displayBrightnessSlider.value = expectedBrightnessValue;
        displayBrightnessSlider.dispatchEvent(
            new CustomEvent('cr-slider-value-changed'));

        const increment = 10;
        // Ensure slider keys work with increments.
        assertEquals(
            expectedBrightnessValue,
            displaySettingsProvider.getInternalDisplayScreenBrightness());
        pressArrowRight(displayBrightnessSlider);
        assertEquals(
            expectedBrightnessValue + increment,
            displaySettingsProvider.getInternalDisplayScreenBrightness());
        pressArrowRight(displayBrightnessSlider);
        assertEquals(
            expectedBrightnessValue + (2 * increment),
            displaySettingsProvider.getInternalDisplayScreenBrightness());
        pressArrowRight(displayBrightnessSlider);
        assertEquals(
            maxOutputBrightnessPercent,
            displaySettingsProvider.getInternalDisplayScreenBrightness());
        // Pressing right arrow when the brightness is already at maximum should
        // maintain the same level.
        pressArrowRight(displayBrightnessSlider);
        assertEquals(
            maxOutputBrightnessPercent,
            displaySettingsProvider.getInternalDisplayScreenBrightness());
        pressArrowLeft(displayBrightnessSlider);
        assertEquals(
            maxOutputBrightnessPercent - increment,
            displaySettingsProvider.getInternalDisplayScreenBrightness());
        pressArrowLeft(displayBrightnessSlider);
        assertEquals(
            maxOutputBrightnessPercent - (2 * increment),
            displaySettingsProvider.getInternalDisplayScreenBrightness());
      });

  test(
      'Auto brightness toggle updates DisplaySettingsProvider, flag enabled',
      async () => {
        loadTimeData.overrideValues(
            {enableDisplayBrightnessControlInSettings: true});
        await initPage();

        // Set up the internal display.
        addDisplay(1);
        fakeSystemDisplay.onDisplayChanged.callListeners();
        await fakeSystemDisplay.getInfoCalled.promise;
        await fakeSystemDisplay.getLayoutCalled.promise;
        assertEquals(1, displayPage.displays.length);
        flush();

        // Before adjusting the auto-brightness toggle, the value in
        // FakeDisplaySettingsProvider should be equal to the default (true).
        assertTrue(displaySettingsProvider
                       .getInternalDisplayAmbientLightSensorEnabled());

        const autoBrightnessToggle =
            displayPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#autoBrightnessToggle');
        assertTrue(!!autoBrightnessToggle);

        // Set the auto-brightness toggle to be on, to match the state of the
        // provider setting.
        // TODO(cambickel): When the auto-brightness toggle can observe the
        // current state of the auto-brightness setting, remove this.
        autoBrightnessToggle.checked = true;
        await flushTasks();

        // Switch auto-brightness to off.
        autoBrightnessToggle.click();
        await flushTasks();

        // The setting in FakeDisplaySettingsProvider should now be disabled.
        assertFalse(displaySettingsProvider
                        .getInternalDisplayAmbientLightSensorEnabled());

        // Switch auto-brightness to on.
        autoBrightnessToggle.click();
        await flushTasks();

        // The setting in FakeDisplaySettingsProvider should now be enabled.
        assertTrue(displaySettingsProvider
                       .getInternalDisplayAmbientLightSensorEnabled());

        // Switch auto-brightness to off by clicking on the row.
        const autoBrightnessToggleRow =
            displayPage.shadowRoot!.querySelector<HTMLDivElement>(
                '#autoBrightnessToggleRow');
        assertTrue(!!autoBrightnessToggleRow);
        autoBrightnessToggleRow.click();
        await flushTasks();

        // The setting in FakeDisplaySettingsProvider should now be disabled.
        assertFalse(displaySettingsProvider
                        .getInternalDisplayAmbientLightSensorEnabled());

        // Switch auto-brightness to on by clicking on the row.
        autoBrightnessToggleRow.click();
        await flushTasks();

        // The setting in FakeDisplaySettingsProvider should now be enabled.
        assertTrue(displaySettingsProvider
                       .getInternalDisplayAmbientLightSensorEnabled());
      });

  test(
      'Auto brightness toggle updates DisplaySettingsProvider, flag enabled',
      async () => {
        loadTimeData.overrideValues(
            {enableDisplayBrightnessControlInSettings: true});

        // Set auto-brightness to initially be disabled.
        const initialAutoBrightnessEnabled = false;
        displaySettingsProvider.setInternalDisplayAmbientLightSensorEnabled(
            initialAutoBrightnessEnabled);

        await initPage();

        // Set up the internal display.
        addDisplay(1);
        fakeSystemDisplay.onDisplayChanged.callListeners();
        await fakeSystemDisplay.getInfoCalled.promise;
        await fakeSystemDisplay.getLayoutCalled.promise;
        assertEquals(1, displayPage.displays.length);
        flush();

        // The auto-brightness toggle should be present on the internal display.
        const displayAutoBrightnessToggle = strictQuery(
            '#autoBrightnessToggle', displayPage.shadowRoot, CrToggleElement);
        assertTrue(!!displayAutoBrightnessToggle);

        // The auto-brightness toggle should initially match the state of the
        // ambient light sensor (not enabled, in this case).
        assertEquals(
            initialAutoBrightnessEnabled, displayAutoBrightnessToggle.checked);

        // Enable the ambient light sensor and notify observers.
        displaySettingsProvider.setInternalDisplayAmbientLightSensorEnabled(
            true);
        displaySettingsProvider.notifyAmbientLightSensorEnabledChanged();
        await flushTasks();

        // The auto-brightness toggle should now be checked, to match the state
        // of the ambient light sensor.
        assertTrue(displayAutoBrightnessToggle.checked);

        // Disable the ambient light sensor and notify observers.
        displaySettingsProvider.setInternalDisplayAmbientLightSensorEnabled(
            false);
        displaySettingsProvider.notifyAmbientLightSensorEnabledChanged();
        await flushTasks();

        // The auto-brightness toggle should now be checked, to match the state
        // of the ambient light sensor.
        assertFalse(displayAutoBrightnessToggle.checked);
      });

});
