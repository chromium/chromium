// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import type {SettingsDetailedBuildInfoSubpageElement} from 'chrome://os-settings/lazy_load.js';
import type {CrPolicyIndicatorElement} from 'chrome://os-settings/os_settings.js';
import {AboutPageBrowserProxyImpl, Router, routes, settingMojom} from 'chrome://os-settings/os_settings.js';
import {CrPolicyIndicatorType} from 'chrome://resources/ash/common/cr_policy_indicator_behavior.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

import {TestAboutPageBrowserProxy} from './test_about_page_browser_proxy.js';

suite('<detailed-build-info-subpage>', () => {
  let page: SettingsDetailedBuildInfoSubpageElement;
  let browserProxy: TestAboutPageBrowserProxy;

  async function createPage(): Promise<void> {
    clearBody();
    page = document.createElement('settings-detailed-build-info-subpage');
    document.body.appendChild(page);
    await flushTasks();
  }

  function deepLinkToSetting(setting: settingMojom.Setting): void {
    const params = new URLSearchParams();
    params.append('settingId', setting.toString());
    Router.getInstance().navigateTo(routes.ABOUT_DETAILED_BUILD_INFO, params);
  }

  function getChangeChannelButton(): HTMLButtonElement {
    const button = page.shadowRoot!.querySelector<HTMLButtonElement>(
        '#changeChannelButton');
    assertTrue(!!button);
    return button;
  }

  function queryChangeChannelPolicyIndicator(): CrPolicyIndicatorElement|null {
    return page.shadowRoot!.querySelector<CrPolicyIndicatorElement>(
        '#changeChannelPolicyIndicator');
  }

  setup(() => {
    loadTimeData.overrideValues({
      aboutEnterpriseManaged: false,
      isManaged: false,
    });

    browserProxy = new TestAboutPageBrowserProxy();
    AboutPageBrowserProxyImpl.setInstanceForTesting(browserProxy);

    Router.getInstance().navigateTo(routes.ABOUT_DETAILED_BUILD_INFO);
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

  test('Browser proxy methods are called on initialization', async () => {
    await createPage();

    await Promise.all([
      browserProxy.whenCalled('pageReady'),
      browserProxy.whenCalled('isConsumerAutoUpdateEnabled'),
      browserProxy.whenCalled('setConsumerAutoUpdate'),
      browserProxy.whenCalled('canChangeChannel'),
      browserProxy.whenCalled('getChannelInfo'),
      browserProxy.whenCalled('getVersionInfo'),
    ]);
  });

  test(
      'Browser proxy methods are called on initialization for managed device',
      async () => {
        loadTimeData.overrideValues({
          isManaged: true,
        });
        await createPage();

        await Promise.all([
          browserProxy.whenCalled('pageReady'),
          browserProxy.whenCalled('isManagedAutoUpdateEnabled'),
          browserProxy.whenCalled('canChangeChannel'),
          browserProxy.whenCalled('getChannelInfo'),
          browserProxy.whenCalled('getVersionInfo'),
        ]);
      });

  [true, false].forEach((canChangeChannel) => {
    suite(
        `When user ${canChangeChannel ? 'can' : 'cannot'} change channel`,
        () => {
          setup(() => {
            browserProxy.setCanChangeChannel(canChangeChannel);
          });

          function assertChangeChannelButtonEnabledState(): void {
            const changeChannelButton = getChangeChannelButton();
            assertEquals(canChangeChannel, !changeChannelButton.disabled);
          }

          function assertChangeChannelPolicyIndicatorVisibility(
              isManaged = false): void {
            const policyIndicator = queryChangeChannelPolicyIndicator();

            if (canChangeChannel) {
              assertFalse(isVisible(policyIndicator));
            } else {
              assertTrue(!!policyIndicator);
              assertTrue(isVisible(policyIndicator));
              assertEquals(
                  isManaged ? CrPolicyIndicatorType.DEVICE_POLICY :
                              CrPolicyIndicatorType.OWNER,
                  policyIndicator.indicatorType);
            }
          }

          test('Change channel button enabled state', async () => {
            await createPage();
            await browserProxy.whenCalled('canChangeChannel');

            assertChangeChannelButtonEnabledState();
          });

          test('Change channel policy indicator visibility', async () => {
            await createPage();
            await browserProxy.whenCalled('canChangeChannel');

            assertChangeChannelPolicyIndicatorVisibility();
          });

          if (!canChangeChannel) {
            test(
                'Change channel policy indicator is visible for managed device',
                async () => {
                  loadTimeData.overrideValues({
                    aboutEnterpriseManaged: true,
                  });
                  await createPage();
                  await browserProxy.whenCalled('canChangeChannel');

                  assertChangeChannelPolicyIndicatorVisibility(
                      /*isManaged=*/ true);
                });
          }

          /**
           * Checks whether the "change channel" button state (enabled/disabled)
           * is correct before getChannelInfo() returns
           * (see https://crbug.com/848750). Here, getChannelInfo() is blocked
           * manually until after the button check.
           */
          test(
              'Change channel button state before getChannelInfo() returns',
              async () => {
                const fakeDelayPromise = new PromiseResolver();
                browserProxy.fakeChannelInfoDelay = fakeDelayPromise;
                await createPage();

                assertChangeChannelButtonEnabledState();
                fakeDelayPromise.resolve(null);
                await browserProxy.whenCalled('getChannelInfo');
              });

          test('Deep link to change channel button', async () => {
            await createPage();

            const setting = settingMojom.Setting.kChangeChromeChannel;
            deepLinkToSetting(setting);

            const changeChannelButton = getChangeChannelButton();
            await waitAfterNextRender(changeChannelButton);
            assertEquals(
                changeChannelButton, page.shadowRoot!.activeElement,
                `Change channel button should be focused for settingId=${
                    setting}.`);
          });
        });
  });

  test('Copy build details button copies info to clipboard', async () => {
    await createPage();
    await Promise.all([
      browserProxy.whenCalled('getVersionInfo'),
      browserProxy.whenCalled('getChannelInfo'),
      browserProxy.whenCalled('canChangeChannel'),
    ]);

    const copyBuildDetailsButton =
        page.shadowRoot!.querySelector<HTMLElement>('#copyBuildDetailsButton');
    assertTrue(!!copyBuildDetailsButton);

    const {osVersion, osFirmware, arcVersion} =
        browserProxy.getVersionInfoForTesting();
    const {targetChannel} = browserProxy.getChannelInfoForTesting();
    const expectedClipBoardText =
        `${loadTimeData.getString('application_label')}: ` +
        `${loadTimeData.getString('aboutBrowserVersion')}\n` +
        `Platform: ${osVersion}\n` +
        `Channel: ${targetChannel}\n` +
        `Firmware Version: ${osFirmware}\n` +
        `ARC Enabled: ${loadTimeData.getBoolean('aboutIsArcEnabled')}\n` +
        `ARC: ${arcVersion}\n` +
        'Enterprise Enrolled: ' +
        `${loadTimeData.getBoolean('aboutEnterpriseManaged')}\n` +
        'Developer Mode: ' +
        `${loadTimeData.getBoolean('aboutIsDeveloperMode')}`;

    let clipboardText = await navigator.clipboard.readText();
    assertEquals('', clipboardText);

    copyBuildDetailsButton.click();
    clipboardText = await navigator.clipboard.readText();
    assertEquals(expectedClipBoardText, clipboardText);
  });
});
