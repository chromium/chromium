// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/appearance.js';

import type {AppearanceElement} from 'chrome://customize-chrome-side-panel.top-chrome/appearance.js';
import {CustomizeChromeAction} from 'chrome://customize-chrome-side-panel.top-chrome/common.js';
import type {CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import type {ManagedDialogElement} from 'chrome://resources/cr_components/managed_dialog/managed_dialog.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

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

  test('announce default theme starting with non-default theme', async () => {
    loadTimeData.overrideValues({
      updatedToClassicChrome: 'Theme updated to Classic Chrome',
    });

    // Add listener to count announcements.
    let announcementCount = 0;
    const updateAnnouncementCount = () => {
      announcementCount += 1;
    };
    document.body.addEventListener(
        'cr-a11y-announcer-messages-sent', updateAnnouncementCount);
    const announcementPromise =
        eventToPromise('cr-a11y-announcer-messages-sent', document.body);

    // Set non-classic chrome theme.
    let theme = createTheme();
    theme.backgroundImage = createBackgroundImage('chrome://theme/foo');

    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    // Set classic chrome. This should announce.
    theme = createTheme();

    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    const announcement = await announcementPromise;
    assertEquals(announcementCount, 1);
    assertTrue(!!announcement);
    assertTrue(announcement.detail.messages.includes(
        'Theme updated to Classic Chrome'));

    // Cleanup event listener.
    document.body.removeEventListener(
        'cr-a11y-announcer-messages-sent', updateAnnouncementCount);
  });

  test('announce default theme starting with default theme', async () => {
    loadTimeData.overrideValues({
      updatedToClassicChrome: 'Theme updated to Classic Chrome',
    });

    // Add listener to count announcements.
    let announcementCount = 0;
    const updateAnnouncementCount = () => {
      announcementCount += 1;
    };
    document.body.addEventListener(
        'cr-a11y-announcer-messages-sent', updateAnnouncementCount);
    const announcementPromise =
        eventToPromise('cr-a11y-announcer-messages-sent', document.body);

    // Set initial classic chrome, which should not announce, since
    // it is the initial theme.
    let theme = createTheme();

    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    // Set non-classic chrome theme.
    theme = createTheme();
    theme.backgroundImage = createBackgroundImage('chrome://theme/foo');

    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    // Set classic chrome. This should announce.
    theme = createTheme();

    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    const announcement = await announcementPromise;
    assertEquals(announcementCount, 1);
    assertTrue(!!announcement);
    assertTrue(announcement.detail.messages.includes(
        'Theme updated to Classic Chrome'));

    // Cleanup event listener.
    document.body.removeEventListener(
        'cr-a11y-announcer-messages-sent', updateAnnouncementCount);
  });

  test(
      'move focus to edit theme when classic chrome button disappears',
      async () => {
        // Arrange.
        let theme = createTheme();
        theme.backgroundImage = createBackgroundImage('chrome://theme/foo');

        callbackRouterRemote.setTheme(theme);
        await callbackRouterRemote.$.flushForTesting();

        // Act.
        appearanceElement.$.setClassicChromeButton.focus();
        assertEquals(
            appearanceElement.shadowRoot!.activeElement,
            appearanceElement.$.setClassicChromeButton);

        theme = createTheme();
        callbackRouterRemote.setTheme(theme);
        await callbackRouterRemote.$.flushForTesting();

        // Assert.
        assertEquals(
            appearanceElement.shadowRoot!.activeElement,
            appearanceElement.$.editThemeButton);
      });

  test(
      'do not move focus if not on classic chrome button when set',
      async () => {
        // Arrange.
        let theme = createTheme();
        theme.backgroundImage = createBackgroundImage('chrome://theme/foo');

        callbackRouterRemote.setTheme(theme);
        await callbackRouterRemote.$.flushForTesting();
        const focusedElement = appearanceElement.shadowRoot!.activeElement;
        assertNotEquals(
            focusedElement, appearanceElement.$.setClassicChromeButton);

        // Act.
        theme = createTheme();

        callbackRouterRemote.setTheme(theme);
        await callbackRouterRemote.$.flushForTesting();

        // Assert.
        assertEquals(
            appearanceElement.shadowRoot!.activeElement, focusedElement);
      });

  test('1P view shows when 3P theme info not set', async () => {
    const theme = createTheme();

    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    assertNotStyle(appearanceElement.$.themeSnapshot, 'display', 'none');
    assertNotStyle(appearanceElement.$.chromeColors, 'display', 'none');
    assertStyle(
        appearanceElement.$.thirdPartyThemeLinkButton, 'display', 'none');
  });

  test('respects policy for edit theme', async () => {
    const theme = createTheme();
    theme.backgroundManagedByPolicy = true;
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    appearanceElement.$.editThemeButton.click();
    await microtasksFinished();

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
    await microtasksFinished();

    appearanceElement.$.setClassicChromeButton.click();
    await microtasksFinished();

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
          appearanceElement.$.thirdPartyThemeLinkButton, 'display', 'none');
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
      appearanceElement.$.thirdPartyThemeLinkButton.click();
      assertEquals(1, handler.getCallCount('openThirdPartyThemePage'));
    });
  });

  test('uploaded image shows button and no snapshot', async () => {
    const theme = createTheme();
    theme.backgroundImage = createBackgroundImage('local');
    theme.backgroundImage.isUploadedImage = true;

    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    assertStyle(
        appearanceElement.$.thirdPartyThemeLinkButton, 'display', 'none');
    assertNotStyle(appearanceElement.$.uploadedImageButton, 'display', 'none');
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

    assertNotStyle(appearanceElement.$.uploadedImageButton, 'display', 'none');
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

    assertStyle(
        appearanceElement.$.thirdPartyThemeLinkButton, 'display', 'none');
    assertStyle(appearanceElement.$.uploadedImageButton, 'display', 'none');
    assertNotStyle(appearanceElement.$.searchedImageButton, 'display', 'none');
    assertNotStyle(
        appearanceElement.$.setClassicChromeButton, 'display', 'none');
    assertStyle(appearanceElement.$.themeSnapshot, 'display', 'none');
    assertNotStyle(appearanceElement.$.chromeColors, 'display', 'none');
  });

  test('searched image button opens themes when feature disabled', async () => {
    const clickEvent = eventToPromise('edit-theme-click', appearanceElement);
    appearanceElement.$.searchedImageButton.click();
    await clickEvent;
  });

  suite('WallpaperSearch', async () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        'wallpaperSearchEnabled': true,
      });
    });

    test(
        'searched image button opens wallpaper search when feature enabled',
        async () => {
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

    test('Clicking set to classic Chrome button sets metric', async () => {
      const theme = createTheme();
      theme.backgroundImage = createBackgroundImage('chrome://theme/foo');

      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      appearanceElement.$.setClassicChromeButton.click();

      assertEquals(
          1, metrics.count('NewTabPage.CustomizeChromeSidePanelAction'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.CustomizeChromeSidePanelAction',
              CustomizeChromeAction.SET_CLASSIC_CHROME_THEME_CLICKED));
    });
  });

  suite('WallpaperSearchButton', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        wallpaperSearchButtonEnabled: true,
        categoriesHeader: 'wallpaper search button enabled',
        changeTheme: 'wallpaper search button disabled',
      });
    });

    test('wallpaper search button shows if it is enabled', () => {
      // Both edit buttons show.
      assertTrue(!!appearanceElement.shadowRoot!.querySelector(
          '#wallpaperSearchButton'));
      assertTrue(!!appearanceElement.$.editThemeButton);
      // Buttons share space in their parent container.
      const editThemeButtonWidth =
          appearanceElement.$.editThemeButton.offsetWidth;
      const wallpaperSearchButtonWidth =
          $$<HTMLElement>(
              appearanceElement, '#wallpaperSearchButton')!.offsetWidth;
      assertTrue(wallpaperSearchButtonWidth > 0 && editThemeButtonWidth > 0);
      const editButtonsContainerWidthMinusGap =
          $$<HTMLElement>(
              appearanceElement, '#editButtonsContainer')!.offsetWidth -
          8;
      assertEquals(
          editButtonsContainerWidthMinusGap,
          editThemeButtonWidth + wallpaperSearchButtonWidth);
      // Only wallpaper search button has an icon.
      assertNotStyle(
          $$(appearanceElement, '#wallpaperSearchButton .edit-theme-icon')!,
          'display', 'none');
      assertStyle(
          $$(appearanceElement, '#editThemeButton .edit-theme-icon')!,
          'display', 'none');
      // Edit theme button shows the right text.
      assertEquals(
          appearanceElement.$.editThemeButton.textContent!.trim(),
          'wallpaper search button enabled');
    });

    test(
        'clicking wallpaper search button creates event and sets metric',
        async () => {
          const eventPromise =
              eventToPromise('wallpaper-search-click', appearanceElement);

          $$<HTMLElement>(appearanceElement, '#wallpaperSearchButton')!.click();

          const event = await eventPromise;
          assertTrue(!!event);
          assertEquals(
              1, metrics.count('NewTabPage.CustomizeChromeSidePanelAction'));
          assertEquals(
              1,
              metrics.count(
                  'NewTabPage.CustomizeChromeSidePanelAction',
                  CustomizeChromeAction
                      .WALLPAPER_SEARCH_APPEARANCE_BUTTON_CLICKED));
        });

    test('respects policy for wallpaper search button', async () => {
      const theme = createTheme();
      theme.backgroundManagedByPolicy = true;
      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      $$<HTMLElement>(appearanceElement, '#wallpaperSearchButton')!.click();
      await microtasksFinished();

      const managedDialog =
          $$<ManagedDialogElement>(appearanceElement, 'managed-dialog');
      assertTrue(!!managedDialog);
      assertTrue(managedDialog.$.dialog.open);
    });

    suite('ButtonDisabled', () => {
      suiteSetup(() => {
        loadTimeData.overrideValues({
          wallpaperSearchButtonEnabled: false,
        });
      });

      test('wallpaper search button is not shown if it is disabled', () => {
        // Only edit theme button shows.
        assertFalse(!!appearanceElement.shadowRoot!.querySelector(
            '#wallpaperSearchButton'));
        assertTrue(!!appearanceElement.$.editThemeButton);
        // Edit theme button takes up the full container.
        assertEquals(
            $$<HTMLElement>(
                appearanceElement, '#editButtonsContainer')!.offsetWidth,
            appearanceElement.$.editThemeButton.offsetWidth);
        // Edit theme button shows an icon with the correct text.
        assertNotStyle(
            $$(appearanceElement, '#editThemeButton .edit-theme-icon')!,
            'display', 'none');
        assertEquals(
            $$<HTMLElement>(
                appearanceElement, '#editThemeButton')!.textContent!.trim(),
            'wallpaper search button disabled');
      });
    });
  });

  test('isSourceTabFirstPartyNtp should update the content', async () => {
    const idsControlledByIsSourceTabFirstPartyNtp = [
      '#editButtonsContainer',
      '#themeSnapshot',
    ];

    const idsNotControlledByIsSourceTabFirstPartyNtp = [
      '#thirdPartyThemeLinkButton',
      '#uploadedImageButton',
      '#searchedImageButton',
      '#chromeColors',
      '#followThemeToggle',
      '#followThemeToggleControl',
      '#setClassicChromeButton',
      '#editThemeButton',
      '#editThemeIcon',
    ];

    const checkIdsVisibility = (isSourceTabFirstPartyNtp: boolean) => {
      idsControlledByIsSourceTabFirstPartyNtp.forEach(
          id => assertEquals(
              isSourceTabFirstPartyNtp,
              !!appearanceElement.shadowRoot!.querySelector(id)));
      idsNotControlledByIsSourceTabFirstPartyNtp.forEach(
          id => assertTrue(!!appearanceElement.shadowRoot!.querySelector(id)));
    };

    await[true, false].forEach(async b => {
      callbackRouterRemote.attachedTabStateUpdated(b);
      await microtasksFinished();
      checkIdsVisibility(b);
    });
  });
});
