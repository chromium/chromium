// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppLanguageSelectionDialogElement, AppLanguageSelectionItemElement} from 'chrome://os-settings/lazy_load.js';
import {AppLanguageSelectionDialogEntryPoint, AppManagementStore, CrButtonElement, CrSearchFieldElement, IronListElement} from 'chrome://os-settings/os_settings.js';
import {App, AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {isHidden, setupFakeHandler} from '../../app_management/test_util.js';

type AppConfig = Partial<App>;
// Enum for assertion tags, denoting which item list is under test.
enum ListType {
  SUGGESTED = 'SUGGESTED_LIST',
  FILTERED = 'FILTERED_LIST',
}

suite('<app-language-selection-dialog>', () => {
  const appLanguageSelectionItemTag = 'app-language-selection-item' as const;
  const listItemId = '#listItem';
  const ironIconTag = 'iron-icon' as const;
  const deviceLanguageLabel = 'Device language';
  const lastSetAppLocalePrefKey = 'arc.last_set_app_locale';
  const defaultPref: chrome.settingsPrivate.PrefObject = {
    key: lastSetAppLocalePrefKey,
    type: chrome.settingsPrivate.PrefType.STRING,
    value: '',
  };
  let appLanguageSelectionDialog: AppLanguageSelectionDialogElement;
  let fakeHandler: FakePageHandler;
  let searchField: CrSearchFieldElement;
  let confirmButton: CrButtonElement;
  let metrics: MetricsTracker;

  setup(async () => {
    metrics = fakeMetricsPrivate();
    appLanguageSelectionDialog =
        document.createElement('app-language-selection-dialog');

    fakeHandler = setupFakeHandler();
    await flushTasks();
  });

  teardown(() => {
    appLanguageSelectionDialog.remove();
  });

  async function addDialog(
      arcConfig: AppConfig, appId: string,
      lastSetAppLocalePref: chrome.settingsPrivate.PrefObject = defaultPref,
      entryPoint: AppLanguageSelectionDialogEntryPoint =
          AppLanguageSelectionDialogEntryPoint.APPS_MANAGEMENT_PAGE):
      Promise<void> {
    const arcApp: App = await fakeHandler.addApp(appId, arcConfig);
    await fakeHandler.flushPipesForTesting();
    appLanguageSelectionDialog.app = arcApp;
    appLanguageSelectionDialog.entryPoint = entryPoint;
    appLanguageSelectionDialog.prefs = {
      arc: {last_set_app_locale: lastSetAppLocalePref},
    };

    document.body.appendChild(appLanguageSelectionDialog);
    flush();

    assertTrue(
        appLanguageSelectionDialog.$.dialog.open, 'Dialog is not opened');
    const suggestedList = getSuggestedList();
    assertTrue(!!suggestedList, '#suggestedItemsList not found');
    const searchFieldTemp =
        appLanguageSelectionDialog.shadowRoot!
            .querySelector<CrSearchFieldElement>('cr-search-field');
    assertTrue(!!searchFieldTemp);
    searchField = searchFieldTemp;

    const confirmButtonTemp =
        appLanguageSelectionDialog.shadowRoot!.querySelector<CrButtonElement>(
            '.action-button');
    assertTrue(!!confirmButtonTemp);
    confirmButton = confirmButtonTemp;
  }

  function getSuggestedList(): IronListElement {
    return appLanguageSelectionDialog.shadowRoot!
        .querySelector<IronListElement>('#suggestedItemsList')!;
  }

  function getFilteredList(): IronListElement|null {
    const filteredList =
        appLanguageSelectionDialog.shadowRoot!.querySelector<IronListElement>(
            '#filteredItemsList');
    return filteredList;
  }

  function getSuggestedItems(): NodeListOf<AppLanguageSelectionItemElement> {
    return getSuggestedList().querySelectorAll(appLanguageSelectionItemTag);
  }

  function getFilteredItems(): NodeListOf<AppLanguageSelectionItemElement> {
    const filteredList = getFilteredList();
    assertTrue(
        !!filteredList,
        'Filtered list should be present when fetching filtered items.');
    return filteredList.querySelectorAll(appLanguageSelectionItemTag);
  }

  function assertLanguageItem(
      list: NodeListOf<AppLanguageSelectionItemElement>, idx: number,
      textContent: string, isSelected: boolean, listType: ListType): void {
    const tag = `[${listType}-item#${idx}]`;
    assertTrue(
        idx < list.length,
        `${tag} Invalid idx ${idx} is larger than list size ${list.length}`);
    const languageItem = list[idx]!.shadowRoot!.querySelector(listItemId);
    assertTrue(!!languageItem, `${tag} #list-item not found`);
    assertTrue(!!languageItem.textContent, `${tag} Item textContent not found`);
    assertTrue(
        languageItem.textContent.includes(textContent),
        `${tag} Invalid text ${languageItem.textContent}`);
    if (isSelected) {
      assertTrue(
          isVisible(languageItem.querySelector(ironIconTag)),
          `${tag} Selection icon is not visible`);
    } else {
      assertTrue(
          isHidden(languageItem.querySelector(ironIconTag)),
          `${tag} Selection icon is not hidden`);
    }
  }

  function setSearchQuery(query: string): void {
    searchField.setValue(query);
    flush();
  }

  function getNoSearchResultField(): HTMLDivElement|null {
    return appLanguageSelectionDialog.shadowRoot!.querySelector<HTMLDivElement>(
        '#noSearchResults');
  }


  test('No selected locale, device language is pre-selected', async () => {
    const testDisplayName = 'testDisplayName';
    const arcOptions: AppConfig = {
      type: AppType.kArc,
      supportedLocales: [{
        localeTag: 'test1',
        displayName: testDisplayName,
        nativeDisplayName: '',
      }],
    };
    await addDialog(arcOptions, 'no-selected-locale');

    // Suggested item should only contain device language, and selected.
    const suggestedItems = getSuggestedItems();
    assertEquals(1, suggestedItems.length);
    assertLanguageItem(
        suggestedItems, /* idx= */ 0, deviceLanguageLabel,
        /* isSelected= */ true, ListType.SUGGESTED);
    // Filtered item should contain the supported locale, but not selected.
    const filteredItems = getFilteredItems();
    assertEquals(1, filteredItems.length);
    assertLanguageItem(
        filteredItems, /* idx= */ 0, testDisplayName, /* isSelected= */ false,
        ListType.FILTERED);
  });

  test(
      'No selected locale with multiple supported locales, displayName ' +
          'and nativeDisplayName is merged when possible',
      async () => {
        const sameDisplayName = 'sameDisplayName';
        const testDisplayName = 'testDisplayName';
        const testNativeDisplayName = 'testNativeDisplayName';
        const arcOptions: AppConfig = {
          type: AppType.kArc,
          supportedLocales: [
            {
              localeTag: 'test1',
              displayName: sameDisplayName,
              nativeDisplayName: sameDisplayName,
            },
            {
              localeTag: 'test2',
              displayName: testDisplayName,
              nativeDisplayName: testNativeDisplayName,
            },
          ],
        };
        await addDialog(arcOptions, 'no-selected-locale-multiple-locales');

        const filteredItems = getFilteredItems();
        assertEquals(2, filteredItems.length);
        // If displayName and nativeDisplayName is same, only use displayName.
        assertLanguageItem(
            filteredItems, /* idx= */ 0, sameDisplayName,
            /* isSelected= */ false, ListType.FILTERED);
        // Name should be concatenated (displayName - nativeDisplayName).
        assertLanguageItem(
            filteredItems, /* idx= */ 1,
            testDisplayName + ' - ' + testNativeDisplayName,
            /* isSelected= */ false, ListType.FILTERED);
      });

  test(
      'Selected locale with one supported locale, selectedLocale should ' +
          'be pre-selected and filteredItems empty',
      async () => {
        const testDisplayName = 'testDisplayName';
        const arcOptions: AppConfig = {
          type: AppType.kArc,
          supportedLocales: [{
            localeTag: 'test1',
            displayName: testDisplayName,
            nativeDisplayName: '',
          }],
          selectedLocale: {
            localeTag: 'test1',
            displayName: testDisplayName,
            nativeDisplayName: '',
          },
        };
        await addDialog(arcOptions, 'selected-locale-one-supported-locale');

        // Suggested items should contain deviceLanguage and selectedLocale.
        const suggestedItems = getSuggestedItems();
        assertEquals(2, suggestedItems.length);
        // Device language shouldn't be selected.
        assertLanguageItem(
            suggestedItems, /* idx= */ 0, deviceLanguageLabel,
            /* isSelected= */ false, ListType.SUGGESTED);
        // Test item should be selected.
        assertLanguageItem(
            suggestedItems, /* idx= */ 1, testDisplayName,
            /* isSelected= */ true, ListType.SUGGESTED);
        // Filtered list should be hidden.
        assertTrue(
            isHidden(getFilteredList()), '#filteredItemsList is not hidden');
      });

  test(
      'Selected locale with multiple supported locales, filteredItems ' +
          'should not be empty',
      async () => {
        const testDisplayName = 'testDisplayName';
        const testDisplayName2 = 'testDisplayName2';
        const arcOptions: AppConfig = {
          type: AppType.kArc,
          supportedLocales: [
            {
              localeTag: 'test1',
              displayName: testDisplayName,
              nativeDisplayName: '',
            },
            {
              localeTag: 'test2',
              displayName: testDisplayName2,
              nativeDisplayName: '',
            },
          ],
          selectedLocale: {
            localeTag: 'test1',
            displayName: testDisplayName,
            nativeDisplayName: '',
          },
        };
        await addDialog(
            arcOptions, 'selected-locale-multiple-supported-locales');

        const filteredItems = getFilteredItems();
        assertEquals(1, filteredItems.length);
        // Selected item should be in the suggestedList
        assertLanguageItem(
            filteredItems, /* idx= */ 0, testDisplayName2,
            /* isSelected= */ false, ListType.FILTERED);
      });

  test(
      'Toggle to device language, selectedLocale should move to ' +
          'device language',
      async () => {
        const testDisplayName = 'testDisplayName';
        const arcOptions: AppConfig = {
          type: AppType.kArc,
          supportedLocales: [{
            localeTag: 'test1',
            displayName: testDisplayName,
            nativeDisplayName: '',
          }],
          selectedLocale: {
            localeTag: 'test1',
            displayName: testDisplayName,
            nativeDisplayName: '',
          },
        };
        await addDialog(arcOptions, 'toggle-to-device-language');

        const suggestedItems = getSuggestedItems();
        assertEquals(2, suggestedItems.length);
        assertLanguageItem(
            suggestedItems, /* idx= */ 0, deviceLanguageLabel,
            /* isSelected= */ false, ListType.SUGGESTED);
        suggestedItems[0]!.shadowRoot!
            .querySelector<HTMLDivElement>(listItemId)!.click();
        // Device language should be selected.
        assertLanguageItem(
            suggestedItems, /* idx= */ 0, deviceLanguageLabel,
            /* isSelected= */ true, ListType.SUGGESTED);
        // The other item should be un-selected.
        assertLanguageItem(
            suggestedItems, /* idx= */ 1, testDisplayName,
            /* isSelected= */ false, ListType.SUGGESTED);
      });

  test(
      'Filter with search query, hide list and only matching items are visible',
      async () => {
        const testDisplayName = 'abc';
        const testDisplayName2 = 'abcde';
        const arcOptions: AppConfig = {
          type: AppType.kArc,
          supportedLocales: [
            {
              localeTag: 'test1',
              displayName: testDisplayName,
              nativeDisplayName: '',
            },
            {
              localeTag: 'test2',
              displayName: testDisplayName2,
              nativeDisplayName: '',
            },
          ],
          selectedLocale: {
            localeTag: 'test1',
            displayName: testDisplayName,
            nativeDisplayName: '',
          },
        };
        await addDialog(arcOptions, 'filter-with-search-query');

        setSearchQuery('abcde');
        // SuggestedList should be hidden.
        assertTrue(isHidden(getSuggestedList()));
        let filteredItems = getFilteredItems();
        // Only second item should be shown.
        assertEquals(1, filteredItems.length);
        assertLanguageItem(
            filteredItems, /* idx= */ 0, testDisplayName2,
            /* isSelected= */ false, ListType.FILTERED);
        assertTrue(isHidden(getNoSearchResultField()));

        setSearchQuery('abc');
        // Two items should be shown with the first one selected.
        filteredItems = getFilteredItems();
        assertEquals(2, filteredItems.length);
        assertLanguageItem(
            filteredItems, /* idx= */ 0, testDisplayName,
            /* isSelected= */ true, ListType.FILTERED);
        assertLanguageItem(
            filteredItems, /* idx= */ 1, testDisplayName2,
            /* isSelected= */ false, ListType.FILTERED);
        assertTrue(isHidden(getNoSearchResultField()));

        // No language matches search query, "no-search-result" text
        // should be shown.
        setSearchQuery('abd');
        assertTrue(isHidden(getFilteredList()));
        assertTrue(isVisible(getNoSearchResultField()));
      });

  test(
      'Confirm selection, selectedLocale should be set to test locale',
      async () => {
        const appId = 'confirm-selection';
        const testLocaleTag = 'testLocaleTag';
        const testDisplayName = 'testDisplayName';
        const arcOptions: AppConfig = {
          type: AppType.kArc,
          supportedLocales: [{
            localeTag: testLocaleTag,
            displayName: testDisplayName,
            nativeDisplayName: '',
          }],
        };
        await addDialog(arcOptions, appId);

        const filteredItems = getFilteredItems();
        assertEquals(1, filteredItems.length);
        filteredItems[0]!.shadowRoot!.querySelector<HTMLDivElement>(
                                         listItemId)!.click();
        // Test language should be selected.
        assertLanguageItem(
            filteredItems, /* idx= */ 0, testDisplayName,
            /* isSelected= */ true, ListType.FILTERED);
        confirmButton.click();
        await fakeHandler.flushPipesForTesting();

        const app = AppManagementStore.getInstance().data.apps[appId];
        assertEquals(testLocaleTag, app!.selectedLocale!.localeTag);
      });

  test(
      'Open dialog from AppsManagementPage and confirm selection, ' +
          'metrics recorded',
      async () => {
        const appId = 'open-dialog-from-apps-management-page';
        const testLocaleTag = 'testLocaleTag';
        const testDisplayName = 'testDisplayName';
        const arcOptions: AppConfig = {
          type: AppType.kArc,
          supportedLocales: [{
            localeTag: testLocaleTag,
            displayName: testDisplayName,
            nativeDisplayName: '',
          }],
        };
        await addDialog(
            arcOptions, appId, defaultPref,
            AppLanguageSelectionDialogEntryPoint.APPS_MANAGEMENT_PAGE);

        const filteredItems = getFilteredItems();
        assertEquals(1, filteredItems.length);
        filteredItems[0]!.shadowRoot!.querySelector<HTMLElement>(
                                         listItemId)!.click();
        // Test language should be selected.
        assertLanguageItem(
            filteredItems, /* idx= */ 0, testDisplayName,
            /* isSelected= */ true, ListType.FILTERED);
        confirmButton.click();
        await fakeHandler.flushPipesForTesting();

        assertEquals(
            1,
            metrics.count(
                'Arc.AppLanguageSwitch.AppsManagementPage.TargetLanguage',
                testLocaleTag));
      });
  test(
      'Open dialog from LanguagesPage and confirm selection, ' +
          'metrics recorded',
      async () => {
        const appId = 'open-dialog-from-languages-page';
        const testLocaleTag = 'testLocaleTag';
        const testDisplayName = 'testDisplayName';
        const arcOptions: AppConfig = {
          type: AppType.kArc,
          supportedLocales: [{
            localeTag: testLocaleTag,
            displayName: testDisplayName,
            nativeDisplayName: '',
          }],
        };
        await addDialog(
            arcOptions, appId, defaultPref,
            AppLanguageSelectionDialogEntryPoint.LANGUAGES_PAGE);

        const filteredItems = getFilteredItems();
        assertEquals(1, filteredItems.length);
        filteredItems[0]!.shadowRoot!.querySelector<HTMLElement>(
                                         listItemId)!.click();
        // Test language should be selected.
        assertLanguageItem(
            filteredItems, /* idx= */ 0, testDisplayName,
            /* isSelected= */ true, ListType.FILTERED);
        confirmButton.click();
        await fakeHandler.flushPipesForTesting();

        assertEquals(
            1,
            metrics.count(
                'Arc.AppLanguageSwitch.LanguagesPage.TargetLanguage',
                testLocaleTag));
      });

  test(
      'Last set app locale exists with no selected locale, ' +
          'display in suggested locales',
      async () => {
        const appId = 'last-set-app-locale-exists-with-no-selected-locale';
        const testLocaleTag = 'testLocaleTag';
        const testDisplayName = 'testDisplayName';
        const pref: chrome.settingsPrivate.PrefObject = {
          key: lastSetAppLocalePrefKey,
          type: chrome.settingsPrivate.PrefType.STRING,
          value: testLocaleTag,
        };
        const arcOptions: AppConfig = {
          type: AppType.kArc,
          supportedLocales: [{
            localeTag: testLocaleTag,
            displayName: testDisplayName,
            nativeDisplayName: '',
          }],
        };
        await addDialog(arcOptions, appId, pref);

        // Suggested items should contain device language and last set app
        // locale.
        const suggestedItems = getSuggestedItems();
        assertEquals(2, suggestedItems.length);
        assertLanguageItem(
            suggestedItems, /* idx= */ 0, deviceLanguageLabel,
            /* isSelected= */ true, ListType.SUGGESTED);
        assertLanguageItem(
            suggestedItems, /* idx= */ 1, testDisplayName,
            /* isSelected= */ false, ListType.SUGGESTED);
        // Filtered list should be hidden.
        assertTrue(
            isHidden(getFilteredList()), '#filteredItemsList is not hidden');
      });

  test(
      'Last set app locale is same with selected locale, no duplicates',
      async () => {
        const appId = 'last-set-app-locale-is-same-with-selected-locale';
        const testLocaleTag = 'testLocaleTag';
        const testDisplayName = 'testDisplayName';
        const pref: chrome.settingsPrivate.PrefObject = {
          key: lastSetAppLocalePrefKey,
          type: chrome.settingsPrivate.PrefType.STRING,
          value: testLocaleTag,
        };
        const arcOptions: AppConfig = {
          type: AppType.kArc,
          supportedLocales: [{
            localeTag: testLocaleTag,
            displayName: testDisplayName,
            nativeDisplayName: '',
          }],
          selectedLocale: {
            localeTag: testLocaleTag,
            displayName: testDisplayName,
            nativeDisplayName: '',
          },
        };
        await addDialog(arcOptions, appId, pref);

        // Suggested items should contain device language and selected locale.
        const suggestedItems = getSuggestedItems();
        assertEquals(2, suggestedItems.length);
        assertLanguageItem(
            suggestedItems, /* idx= */ 0, deviceLanguageLabel,
            /* isSelected= */ false, ListType.SUGGESTED);
        assertLanguageItem(
            suggestedItems, /* idx= */ 1, testDisplayName,
            /* isSelected= */ true, ListType.SUGGESTED);
        // Filtered list should be hidden.
        assertTrue(
            isHidden(getFilteredList()), '#filteredItemsList is not hidden');
      });

  test(
      'Last set app locale is different with selected locale, ' +
          'display 3 locales in sugested locales',
      async () => {
        const appId = 'last-set-app-locale-is-different-with-selected-locale';
        const testLocaleTag = 'testLocaleTag';
        const testLocaleTag2 = 'testLocaleTag2';
        const testDisplayName = 'testDisplayName';
        const testDisplayName2 = 'testDisplayName2';
        const pref: chrome.settingsPrivate.PrefObject = {
          key: lastSetAppLocalePrefKey,
          type: chrome.settingsPrivate.PrefType.STRING,
          value: testLocaleTag2,
        };
        const arcOptions: AppConfig = {
          type: AppType.kArc,
          supportedLocales: [
            {
              localeTag: testLocaleTag,
              displayName: testDisplayName,
              nativeDisplayName: '',
            },
            {
              localeTag: testLocaleTag2,
              displayName: testDisplayName2,
              nativeDisplayName: '',
            },
          ],
          selectedLocale: {
            localeTag: testLocaleTag,
            displayName: testDisplayName,
            nativeDisplayName: '',
          },
        };
        await addDialog(arcOptions, appId, pref);

        // Suggested items should contain device language, selected locale and
        // last set app locale.
        const suggestedItems = getSuggestedItems();
        assertEquals(3, suggestedItems.length);
        assertLanguageItem(
            suggestedItems, /* idx= */ 0, deviceLanguageLabel,
            /* isSelected= */ false, ListType.SUGGESTED);
        assertLanguageItem(
            suggestedItems, /* idx= */ 1, testDisplayName,
            /* isSelected= */ true, ListType.SUGGESTED);
        assertLanguageItem(
            suggestedItems, /* idx= */ 2, testDisplayName2,
            /* isSelected= */ false, ListType.SUGGESTED);
        // Filtered list should be hidden.
        assertTrue(
            isHidden(getFilteredList()), '#filteredItemsList is not hidden');
      });
});
