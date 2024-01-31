// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Browser tests for the System preferences page.
 *
 * - This suite expects the OsSettingsRevampWayfinding feature flag to be
 *   enabled.
 */

import 'chrome://os-settings/os_settings.js';

import {createRouterForTesting, ensureLazyLoaded, OneDriveBrowserProxy, OsSettingsRoutes, OsSettingsSubpageElement, Route, Router, routes, SettingsSystemPreferencesPageElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {OneDriveTestBrowserProxy} from '../os_files_page/one_drive_test_browser_proxy.js';

interface SubpageData {
  routeName: keyof OsSettingsRoutes;
  elementTagName: string;
}

suite('<settings-system-preferences-page>', () => {
  let page: SettingsSystemPreferencesPageElement;

  async function createPage() {
    page = document.createElement('settings-system-preferences-page');
    document.body.appendChild(page);
    await flushTasks();
  }

  /**
   * Recreate routes (via new Router instance) based on the given load-time
   * data overrides.
   */
  function recreateRoutesFromLoadTimeOverrides(
      overrides: Record<string, boolean>) {
    loadTimeData.overrideValues(overrides);
    const testRouter = createRouterForTesting();
    Router.resetInstanceForTesting(testRouter);
  }

  async function navigateToSubpage(route: Route) {
    await ensureLazyLoaded();
    Router.getInstance().navigateTo(route);
    await flushTasks();
  }

  /**
   * Expects the os-settings-subpage parent element containing the subpage
   * element with the given tag name to be visible on the page.
   */
  function assertSubpageIsVisible(elementTagName: string) {
    const subpageElement = page.shadowRoot!.querySelector(elementTagName);
    assertTrue(!!subpageElement);
    const subpageParentElement = subpageElement.parentNode as HTMLElement;
    assertTrue(subpageParentElement instanceof OsSettingsSubpageElement);
    assertTrue(
        isVisible(subpageParentElement),
        `${elementTagName} should be visible.`);
  }

  setup(() => {
    loadTimeData.overrideValues({
      isGuest: false,
      showOneDriveSettings: false,
      showOfficeSettings: false,
    });

    Router.getInstance().navigateTo(routes.SYSTEM_PREFERENCES);
  });

  teardown(() => {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  suite('Date and Time subsection', () => {
    test('Date and Time settings card is visible', async () => {
      await createPage();

      const dateTimeSettingsCard =
          page.shadowRoot!.querySelector('date-time-settings-card');
      assertTrue(
          isVisible(dateTimeSettingsCard),
          'Date and Time settings card should be visible.');
    });

    test('Timezone subpage is visible', async () => {
      await createPage();

      await navigateToSubpage(routes.DATETIME_TIMEZONE_SUBPAGE);
      assertSubpageIsVisible('timezone-subpage');
    });
  });

  suite('Files subsection', () => {
    test('Files settings card is visible', async () => {
      await createPage();

      const filesSettingsCard =
          page.shadowRoot!.querySelector('files-settings-card');
      assertTrue(
          isVisible(filesSettingsCard),
          'Files settings card should be visible.');
    });

    suite('for guest users', () => {
      setup(() => {
        loadTimeData.overrideValues({isGuest: true});
      });

      test('Files settings card is not visible', async () => {
        await createPage();

        const filesSettingsCard =
            page.shadowRoot!.querySelector('files-settings-card');
        assertFalse(
            isVisible(filesSettingsCard),
            'Files settings card should not be visible.');
      });
    });

    test('File shares subpage is visible for SMB_SHARES route', async () => {
      await createPage();

      await navigateToSubpage(routes.SMB_SHARES);
      assertSubpageIsVisible('settings-smb-shares-page');
    });

    test('Google Drive subpage is visible for GOOGLE_DRIVE route', async () => {
      assertTrue(!!routes.GOOGLE_DRIVE, 'GOOGLE_DRIVE route should exist');
      await createPage();
      await navigateToSubpage(routes.GOOGLE_DRIVE);
      assertSubpageIsVisible('settings-google-drive-subpage');
    });

    suite('when office settings are available', () => {
      setup(() => {
        recreateRoutesFromLoadTimeOverrides({
          showOneDriveSettings: true,
          showOfficeSettings: true,
        });

        const testOneDriveBrowserProxy =
            new OneDriveTestBrowserProxy({email: 'sample@google.com'});
        OneDriveBrowserProxy.setInstance(testOneDriveBrowserProxy);
      });

      test('Office subpage is visible for OFFICE route', async () => {
        assertTrue(!!routes.OFFICE, 'OFFICE route should exist');
        await createPage();
        await navigateToSubpage(routes.OFFICE);
        assertSubpageIsVisible('settings-office-page');
      });

      test(
          'OneDrive subpage subpage element is visible for ONE_DRIVE route',
          async () => {
            assertTrue(!!routes.ONE_DRIVE, 'ONE_DRIVE route should exist');
            await createPage();
            await navigateToSubpage(routes.ONE_DRIVE);
            assertSubpageIsVisible('settings-one-drive-subpage');
          });
    });
  });

  suite('Languages and Input subsection', () => {
    test('Language settings card is visible', async () => {
      await createPage();

      const languageSettingsCard =
          page.shadowRoot!.querySelector('language-settings-card');
      assertTrue(
          isVisible(languageSettingsCard),
          'Language settings card should be visible.');
    });

    const languageSubpages: SubpageData[] = [
      {
        routeName: 'OS_LANGUAGES_LANGUAGES',
        elementTagName: 'os-settings-languages-page-v2',
      },
    ];
    languageSubpages.forEach(({routeName, elementTagName}) => {
      test(
          `${elementTagName} subpage element is visible for route ${routeName}`,
          async () => {
            await createPage();

            await navigateToSubpage(routes[routeName]);
            assertSubpageIsVisible(elementTagName);
          });
    });
  });

  suite('Multitasking subsection', () => {
    test(
        'Multitasking settings card is visible if feature is allowed',
        async () => {
          loadTimeData.overrideValues({shouldShowMultitasking: true});
          await createPage();

          const multitaskingSettingsCard =
              page.shadowRoot!.querySelector('multitasking-settings-card');
          assertTrue(
              isVisible(multitaskingSettingsCard),
              'Multitasking settings card should be visible.');
        });

    test(
        'Multitasking settings card is not visible if feature is disallowed',
        async () => {
          loadTimeData.overrideValues({shouldShowMultitasking: false});
          await createPage();

          const multitaskingSettingsCard =
              page.shadowRoot!.querySelector('multitasking-settings-card');
          assertFalse(
              isVisible(multitaskingSettingsCard),
              'Multitasking settings card should not be visible.');
        });
  });

  suite('Reset subsection', () => {
    test('Reset settings card is visible if powerwash is allowed', async () => {
      loadTimeData.overrideValues({allowPowerwash: true});
      await createPage();

      const resetSettingsCard =
          page.shadowRoot!.querySelector('reset-settings-card');
      assertTrue(
          isVisible(resetSettingsCard),
          'Reset settings card should be visible.');
    });

    test(
        'Reset settings card is not visible if powerwash is disallowed',
        async () => {
          loadTimeData.overrideValues({allowPowerwash: false});
          await createPage();

          const resetSettingsCard =
              page.shadowRoot!.querySelector('reset-settings-card');
          assertFalse(
              isVisible(resetSettingsCard),
              'Reset settings card should not be visible.');
        });
  });

  suite('Search & Assistant subsection', () => {
    test('Search and Assistant settings card is visible', async () => {
      await createPage();

      const searchAndAssistantSettingsCard =
          page.shadowRoot!.querySelector('search-and-assistant-settings-card');
      assertTrue(
          isVisible(searchAndAssistantSettingsCard),
          'Search and Assistant settings card should be visible.');
    });

    test(
        'Search subpage is visible if quick answers is supported', async () => {
          loadTimeData.overrideValues({isQuickAnswersSupported: true});
          await createPage();

          await navigateToSubpage(routes.SEARCH_SUBPAGE);
          assertSubpageIsVisible('settings-search-subpage');
        });

    test(
        'Search subpage is not stamped if quick answers is not supported',
        async () => {
          loadTimeData.overrideValues({isQuickAnswersSupported: false});
          await createPage();

          await navigateToSubpage(routes.SEARCH_SUBPAGE);
          const subpage =
              page.shadowRoot!.querySelector('settings-search-subpage');
          assertNull(subpage, 'Subpage should not be stamped.');
        });

    test('Assistant subpage is visible if assistant is enabled', async () => {
      loadTimeData.overrideValues({isAssistantAllowed: true});
      await createPage();

      await navigateToSubpage(routes.GOOGLE_ASSISTANT);
      assertSubpageIsVisible('settings-google-assistant-subpage');
    });

    test(
        'Assistant subpage is not stamped if assistant is disabled',
        async () => {
          loadTimeData.overrideValues({isAssistantAllowed: false});
          await createPage();

          await navigateToSubpage(routes.GOOGLE_ASSISTANT);
          const subpage = page.shadowRoot!.querySelector(
              'settings-google-assistant-subpage');
          assertNull(subpage, 'Subpage should not be stamped.');
        });
  });

  suite('Startup subsection', () => {
    suite('When startup settings are available', () => {
      setup(() => {
        loadTimeData.overrideValues({shouldShowStartup: false});
      });

      test('Startup settings card is not visible', async () => {
        await createPage();

        const startupSettingsCard =
            page.shadowRoot!.querySelector('startup-settings-card');
        assertFalse(isVisible(startupSettingsCard));
      });
    });

    suite('When startup settings are not available', () => {
      setup(() => {
        loadTimeData.overrideValues({shouldShowStartup: true});
      });

      test('Startup settings card is visible', async () => {
        await createPage();

        const startupSettingsCard =
            page.shadowRoot!.querySelector('startup-settings-card');
        assertTrue(isVisible(startupSettingsCard));
      });
    });
  });

  suite('Storage and power subsection', () => {
    test('Storage and power settings card is visible', async () => {
      await createPage();

      const storageAndPowerSettingsCard =
          page.shadowRoot!.querySelector('storage-and-power-settings-card');
      assertTrue(
          isVisible(storageAndPowerSettingsCard),
          'Storage and power settings card should be visible.');
    });

    const storageAndPowerSubpages: SubpageData[] = [
      {
        routeName: 'STORAGE',
        elementTagName: 'settings-storage',
      },
      {
        routeName: 'POWER',
        elementTagName: 'settings-power',
      },
    ];
    storageAndPowerSubpages.forEach(({routeName, elementTagName}) => {
      test(
          `${elementTagName} subpage element is visible for route ${routeName}`,
          async () => {
            await createPage();

            await navigateToSubpage(routes[routeName]);
            assertSubpageIsVisible(elementTagName);
          });
    });

    test(
        'External storage subpage is visible if Android external storage ' +
            'is enabled',
        async () => {
          loadTimeData.overrideValues({isExternalStorageEnabled: true});
          await createPage();

          await navigateToSubpage(routes.EXTERNAL_STORAGE_PREFERENCES);
          assertSubpageIsVisible('settings-storage-external');
        });

    test(
        'External storage subpage is not stamped if Android external storage ' +
            'is disabled',
        async () => {
          loadTimeData.overrideValues({isExternalStorageEnabled: false});
          await createPage();

          await navigateToSubpage(routes.EXTERNAL_STORAGE_PREFERENCES);
          const subpage =
              page.shadowRoot!.querySelector('settings-storage-external');
          assertNull(subpage, 'Subpage should not be stamped.');
        });
  });
});
