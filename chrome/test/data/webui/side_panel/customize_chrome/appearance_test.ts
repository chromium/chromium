// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/appearance.js';

import {AppearanceElement} from 'chrome://customize-chrome-side-panel.top-chrome/appearance.js';
import {CustomizeChromeAction} from 'chrome://customize-chrome-side-panel.top-chrome/common.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {ManagedDialogElement} from 'chrome://resources/cr_components/managed_dialog/managed_dialog.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {$$, assertNotStyle, assertStyle, createBackgroundImage, createTheme, createThirdPartyThemeInfo, installMock} from './test_support.js';

suite('AppearanceTest', () => {
  let appearanceElement: AppearanceElement;
  let callbackRouterRemote: CustomizeChromePageRemote;
  let handler: TestMock<CustomizeChromePageHandlerRemote>;
  let metrics: MetricsTracker;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    callbackRouterRemote = CustomizeChromeApiProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
    metrics = fakeMetricsPrivate();
    appearanceElement = document.createElement('customize-chrome-appearance');
    document.body.appendChild(appearanceElement);
  });

  test('appearance edit button creates event', async () => {
    const eventPromise = eventToPromise('edit-theme-click', appearanceElement);
    appearanceElement.$.editThemeButton.click();
    const event = await eventPromise;
    assertTrue(!!event);
  });

  test('classic chrome button shows with background image', async () => {
    const theme = createTheme();
    theme.backgroundImage = createBackgroundImage('chrome://theme/foo');

    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    assertTrue(!appearanceElement.$.setClassicChromeButton.hidden);
  });

  test('classic chrome button does not show with classic chrome', async () => {
    const theme = createTheme();

    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    assertTrue(appearanceElement.$.setClassicChromeButton.hidden);
  });

  test('classic chrome button sets theme to classic chrome', async () => {
    const theme = createTheme();
    theme.backgroundImage = createBackgroundImage('chrome://theme/foo');

    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    appearanceElement.$.setClassicChromeButton.click();
    assertEquals(1, handler.getCallCount('removeBackgroundImage'));
    assertEquals(1, handler.getCallCount('setDefaultColor'));
  });

  test('1P view shows when 3P theme info not set', async () => {
    const theme = createTheme();

    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    assertNotStyle(appearanceElement.$.themeSnapshot, 'display', 'none');
    assertNotStyle(appearanceElement.$.chromeColors, 'display', 'none');
    assertStyle(appearanceElement.$.thirdPartyLinkButton, 'display', 'none');
  });

  test('respects policy for edit theme', async () => {
    const theme = createTheme();
    theme.backgroundManagedByPolicy = true;
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    await waitAfterNextRender(appearanceElement);

    appearanceElement.$.editThemeButton.click();
    await waitAfterNextRender(appearanceElement);

    const managedDialog =
        $$<ManagedDialogElement>(appearanceElement, 'managed-dialog');
    assertTrue(!!managedDialog);
    assertTrue(managedDialog.$.dialog.open);
  });

  test('respects policy for reset', async () => {
    const theme = createTheme();
    theme.backgroundManagedByPolicy = true;
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    await waitAfterNextRender(appearanceElement);

    appearanceElement.$.setClassicChromeButton.click();
    await waitAfterNextRender(appearanceElement);

    const managedDialog =
        $$<ManagedDialogElement>(appearanceElement, 'managed-dialog');
    assertTrue(!!managedDialog);
    assertTrue(managedDialog.$.dialog.open);
    assertEquals(0, handler.getCallCount('setDefaultColor'));
    assertEquals(0, handler.getCallCount('removeBackgroundImage'));
  });

  suite('DisableDeviceTheme', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        'showDeviceThemeToggle': false,
      });
    });

    test(
        'follow theme toggle is hidden when showDeviceThemeToggle is false',
        async () => {
          const theme = createTheme();

          callbackRouterRemote.setTheme(theme);
          await callbackRouterRemote.$.flushForTesting();

          assertTrue(appearanceElement.$.followThemeToggle.hidden);
        });
  });

  suite('ShowDeviceTheme', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        'showDeviceThemeToggle': true,
      });
    });

    test('follow theme toggle responds to theme value', async () => {
      const theme = createTheme();
      theme.followDeviceTheme = true;

      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      assertTrue(appearanceElement.$.followThemeToggleControl.checked);
    });

    test('follow theme toggle triggers setFollowDeviceTheme', async () => {
      const theme = createTheme();
      theme.followDeviceTheme = false;

      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      assertTrue(!appearanceElement.$.followThemeToggleControl.checked);

      const setFollowDeviceTheme = handler.whenCalled('setFollowDeviceTheme');
      appearanceElement.$.followThemeToggleControl.click();
      const followDevice = await setFollowDeviceTheme;

      // Clicking on the toggle should result in a request to stop following the
      // theme.
      assertEquals(1, handler.getCallCount('setFollowDeviceTheme'));
      assertTrue(followDevice);
    });

    test(
        'follow theme toggle is shown when showDeviceThemeToggle is true',
        async () => {
          const theme = createTheme();

          callbackRouterRemote.setTheme(theme);
          await callbackRouterRemote.$.flushForTesting();

          assertTrue(!appearanceElement.$.followThemeToggle.hidden);
        });

    test('follow theme toggle is hidden with third party theme', async () => {
      const theme = createTheme();
      theme.thirdPartyThemeInfo = createThirdPartyThemeInfo('foo', 'bar');

      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      assertTrue(appearanceElement.$.followThemeToggle.hidden);
    });
  });

  suite('showBottomDivider', () => {
    ([
      [true, true],
      [true, false],
      [false, true],
      [false, false],
    ] as boolean[][])
        .forEach(([showClassicChromeButton, showDeviceThemeToggle]) => {
          const showBottomDivider =
              showClassicChromeButton || showDeviceThemeToggle;
          const buttonString =
              `showClassicChromeButton ${showClassicChromeButton}`;
          const toggleString = `showDeviceThemeToggle ${showDeviceThemeToggle}`;

          test(
              `${showBottomDivider} when ${buttonString} and ${toggleString}`,
              async () => {
                loadTimeData.overrideValues({showDeviceThemeToggle});
                const theme = createTheme();
                if (showClassicChromeButton) {
                  theme.backgroundImage =
                      createBackgroundImage('chrome://theme/foo');
                }

                callbackRouterRemote.setTheme(theme);
                await callbackRouterRemote.$.flushForTesting();

                assertNotEquals(
                    appearanceElement.$.setClassicChromeButton.hidden,
                    showClassicChromeButton);
                assertNotEquals(
                    appearanceElement.$.followThemeToggle.hidden,
                    showDeviceThemeToggle);
                assertNotEquals(
                    (appearanceElement.shadowRoot!.querySelectorAll(
                         '.sp-hr')[1]! as HTMLElement)
                        .hidden,
                    showBottomDivider);
              });
        });
  });

  suite('third party theme', () => {
    test('3P view shows when 3P theme info is set', async () => {
      const theme = createTheme();
      theme.thirdPartyThemeInfo = createThirdPartyThemeInfo('foo', 'bar');

      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();
      assertNotStyle(
          appearanceElement.$.thirdPartyLinkButton, 'display', 'none');
      assertNotStyle(
          appearanceElement.$.setClassicChromeButton, 'display', 'none');
      assertStyle(appearanceElement.$.themeSnapshot, 'display', 'none');
      assertStyle(appearanceElement.$.chromeColors, 'display', 'none');
    });

    test('clicking 3P theme link opens theme page', async () => {
      // Arrange.
      const theme = createTheme();
      theme.thirdPartyThemeInfo = createThirdPartyThemeInfo('foo', 'Foo Name');

      // Act.
      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      // Assert.
      appearanceElement.$.thirdPartyLinkButton.click();
      assertEquals(1, handler.getCallCount('openThirdPartyThemePage'));
    });
  });

  suite('GM3', async () => {
    suiteSetup(() => {
      document.documentElement.toggleAttribute('chrome-refresh-2023', true);
    });

    test('uploaded image shows button and no snapshot', async () => {
      const theme = createTheme();
      theme.backgroundImage = createBackgroundImage('local');
      theme.backgroundImage.isUploadedImage = true;

      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      assertStyle(appearanceElement.$.thirdPartyLinkButton, 'display', 'none');
      assertNotStyle(
          appearanceElement.$.uploadedImageButton, 'display', 'none');
      assertStyle(appearanceElement.$.searchedImageButton, 'display', 'none');
      assertNotStyle(
          appearanceElement.$.setClassicChromeButton, 'display', 'none');
      assertStyle(appearanceElement.$.themeSnapshot, 'display', 'none');
      assertNotStyle(appearanceElement.$.chromeColors, 'display', 'none');
    });

    test('uploadedImageButton opens upload image dialog', async () => {
      const theme = createTheme();
      theme.backgroundImage = createBackgroundImage('local');
      theme.backgroundImage.isUploadedImage = true;

      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      assertNotStyle(
          appearanceElement.$.uploadedImageButton, 'display', 'none');
      appearanceElement.$.uploadedImageButton.click();
      assertEquals(1, handler.getCallCount('chooseLocalCustomBackground'));
    });

    test('searched image shows button', async () => {
      const theme = createTheme();
      theme.backgroundImage = createBackgroundImage('searched');
      theme.backgroundImage.isUploadedImage = true;
      theme.backgroundImage.localBackgroundId = {
        low: BigInt(10),
        high: BigInt(20),
      };
      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      assertStyle(appearanceElement.$.thirdPartyLinkButton, 'display', 'none');
      assertStyle(appearanceElement.$.uploadedImageButton, 'display', 'none');
      assertNotStyle(
          appearanceElement.$.searchedImageButton, 'display', 'none');
      assertNotStyle(
          appearanceElement.$.setClassicChromeButton, 'display', 'none');
      assertStyle(appearanceElement.$.themeSnapshot, 'display', 'none');
      assertNotStyle(appearanceElement.$.chromeColors, 'display', 'none');

      const clickEvent =
          eventToPromise('wallpaper-search-click', appearanceElement);
      appearanceElement.$.searchedImageButton.click();
      await clickEvent;
    });
  });

  suite('Metrics', () => {
    test('Clicking edit theme button sets metric', () => {
      appearanceElement.$.editThemeButton.click();

      assertEquals(
          1, metrics.count('NewTabPage.CustomizeChromeSidePanelAction'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.CustomizeChromeSidePanelAction',
              CustomizeChromeAction.EDIT_THEME_CLICKED));
    });
  });
});
