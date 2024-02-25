// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {CrLinkRowElement, CrToggleElement, DevicePageBrowserProxyImpl, DisplayLayoutElement, displaySettingsProviderMojom, Router, routes, setDisplayApiForTesting, setDisplaySettingsProviderForTesting, SettingsDisplayElement, SettingsDropdownMenuElement, SettingsSliderElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush, microTask} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakeSystemDisplay} from '../fake_system_display.js';

import {getFakePrefs} from './device_page_test_util.js';
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
        .then(() => {
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
