// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AudioAndCaptionsPageBrowserProxyImpl, NotificationColor, SettingsAudioAndCaptionsPageElement} from 'chrome://os-settings/lazy_load.js';
import {CrButtonElement, CrSettingsPrefs, Router, routes, settingMojom, SettingsDropdownMenuElement, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestAudioAndCaptionsPageBrowserProxy} from '../device_page/test_audio_and_captions_page_browser_proxy.js';
import {clearBody} from '../utils.js';

suite('<settings-audio-and-captions-page>', () => {
  let page: SettingsAudioAndCaptionsPageElement;
  let prefElement: SettingsPrefsElement;
  let browserProxy: TestAudioAndCaptionsPageBrowserProxy;

  async function initPage() {
    browserProxy = new TestAudioAndCaptionsPageBrowserProxy();
    AudioAndCaptionsPageBrowserProxyImpl.setInstanceForTesting(browserProxy);

    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('settings-audio-and-captions-page');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();
  }

  setup(() => {
    clearBody();
    Router.getInstance().navigateTo(routes.A11Y_AUDIO_AND_CAPTIONS);
  });

  teardown(() => {
    page.remove();
    prefElement.remove();
    Router.getInstance().resetRouteForTesting();
  });

  async function testDeepLink(
      setting: settingMojom.Setting, elementId: string) {
    await initPage();

    const params = new URLSearchParams();
    params.append('settingId', setting.toString());
    Router.getInstance().navigateTo(routes.A11Y_AUDIO_AND_CAPTIONS, params);

    const element = page.shadowRoot!.querySelector<HTMLElement>(elementId);

    assert(element);

    await waitAfterNextRender(element);

    assertEquals(
        element, page.shadowRoot!.activeElement,
        `Element should be focused for settingId=${setting}'`);
  }

  function getFlashNotificationsToggle(): SettingsToggleButtonElement {
    const toggle = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#flashNotificationsToggle');
    assert(!!toggle);
    assertTrue(isVisible(toggle));
    return toggle;
  }

  function getNotificationColorDropdown(): SettingsDropdownMenuElement {
    const dropdown =
        page.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#notificationColorDropdown');
    assert(!!dropdown);
    assertTrue(isVisible(dropdown));
    return dropdown;
  }

  test('no subpages are available in kiosk mode', async () => {
    loadTimeData.overrideValues({
      isKioskModeActive: true,
      showTabletModeShelfNavigationButtonsSettings: true,
    });
    await initPage();

    const subpageLinks = page.shadowRoot!.querySelectorAll('cr-link-row');
    subpageLinks.forEach(subpageLink => assertFalse(isVisible(subpageLink)));
  });

  test('mono audio is deep-linkable', async () => {
    await testDeepLink(settingMojom.Setting.kMonoAudio, '#monoAudioEnabled');
  });

  test('startup sound enabled is deep-linkable', async () => {
    await testDeepLink(
        settingMojom.Setting.kStartupSound, '#startupSoundEnabled');
  });

  if (loadTimeData.getBoolean(
          'isAccessibilityFlashNotificationFeatureEnabled')) {
    test('flash notifications is deep-linkable', async () => {
      await testDeepLink(
          settingMojom.Setting.kFlashNotifications,
          '#flashNotificationsToggle');
    });

    test('flash notifications can be toggled', async () => {
      await initPage();
      const flashNotificationsToggle = getFlashNotificationsToggle();

      let notificationColorDropdown =
          page.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
              '#notificationColorDropdown');
      assertNull(notificationColorDropdown);

      assertFalse(flashNotificationsToggle.checked);
      assertFalse(flashNotificationsToggle.hasAttribute('checked'));
      assertFalse(page.prefs.settings.a11y.flash_notifications_enabled.value);

      flashNotificationsToggle.click();
      await flushTasks();

      assertTrue(flashNotificationsToggle.checked);
      assertTrue(flashNotificationsToggle.hasAttribute('checked'));
      assertTrue(page.prefs.settings.a11y.flash_notifications_enabled.value);

      notificationColorDropdown = getNotificationColorDropdown();
    });

    test('flash notification color can be changed', async () => {
      await initPage();
      const flashNotificationsToggle = getFlashNotificationsToggle();
      flashNotificationsToggle.click();
      await flushTasks();

      const notificationColorDropdown = getNotificationColorDropdown();
      const colorSelectElement =
          notificationColorDropdown.shadowRoot!.querySelector('select');
      assert(!!colorSelectElement);

      // Default: yellow.
      assertEquals(
          NotificationColor.YELLOW,
          page.prefs.settings.a11y.flash_notifications_color.value);
      assertEquals(String(NotificationColor.YELLOW), colorSelectElement.value);

      // Change to pink.
      colorSelectElement.value = String(NotificationColor.PINK);
      colorSelectElement.dispatchEvent(new CustomEvent('change'));
      assertEquals(
          NotificationColor.PINK,
          page.prefs.settings.a11y.flash_notifications_color.value);
    });

    test('flash notifications preview', async () => {
      await initPage();
      const flashNotificationsToggle = getFlashNotificationsToggle();
      flashNotificationsToggle.click();
      await flushTasks();

      const previewButton = page.shadowRoot!.querySelector<CrButtonElement>(
          '#notificationPreviewBtn');
      assert(!!previewButton);
      assertTrue(isVisible(previewButton));
      assertEquals(0, browserProxy.getCallCount('previewFlashNotification'));

      previewButton.click();

      assertEquals(1, browserProxy.getCallCount('previewFlashNotification'));
    });
  } else {
    test(
        'flash notifications not shown when feature flag disabled',
        async () => {
          await initPage();
          const flashNotificationsToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#flashNotificationsToggle');
          assertNull(flashNotificationsToggle);

          const flashNotificationsColorOptionsRow =
              page.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
                  '#flashNotificationsColorOptionsRow');
          assertNull(flashNotificationsColorOptionsRow);
        });
  }
});
