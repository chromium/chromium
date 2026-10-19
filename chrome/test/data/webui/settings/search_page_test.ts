// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import type {DefaultSearchEnginePickerData, SearchEnginesInfo, SettingsSearchEngineListDialogElement, SettingsSearchEngineMoreEnginesDialogElement, SettingsSearchPageElement} from 'chrome://settings/settings.js';
import {SearchEnginesBrowserProxyImpl, SearchEnginesInteractions, Router, routes, resetRouterForTesting, loadTimeData, PrefService, PrefsBrowserProxy} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {createSampleSearchEngine, engineId, TestSearchEnginesBrowserProxy} from './test_search_engines_browser_proxy.js';
// clang-format on

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    {
      key: 'default_search_provider_data.template_url_data',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {},
    },
  ];
}

function generateSearchEngineInfo(): SearchEnginesInfo {
  const searchEngines0 = createSampleSearchEngine(
      {canBeDefault: true, default: true, id: engineId('db:0')});
  const searchEngines1 =
      createSampleSearchEngine({canBeDefault: true, id: engineId('db:1')});
  const searchEngines2 =
      createSampleSearchEngine({canBeDefault: true, id: engineId('db:2')});

  return {
    defaults: [searchEngines0, searchEngines1, searchEngines2],
    actives: [],
    others: [],
    extensions: [],
  };
}

function generateDefaultSearchEnginePickerData():
    DefaultSearchEnginePickerData {
  const searchEngines0 = createSampleSearchEngine(
      {canBeDefault: true, default: true, id: engineId('db:0')});
  const searchEngines1 =
      createSampleSearchEngine({canBeDefault: true, id: engineId('db:1')});
  const searchEngines2 =
      createSampleSearchEngine({canBeDefault: true, id: engineId('db:2')});

  return {
    primary: [searchEngines0, searchEngines1, searchEngines2],
  };
}

suite('SearchPageTests', function() {
  let page: SettingsSearchPageElement;
  let browserProxy: TestSearchEnginesBrowserProxy;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let prefService: PrefService;
  let metrics: MetricsTracker;

  setup(async function() {
    loadTimeData.overrideValues({
      searchSettingsUpdate: false,
      pickerWithMoreEngines: false,
    });
    resetRouterForTesting();

    prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    metrics = fakeMetricsPrivate();
    browserProxy = new TestSearchEnginesBrowserProxy();
    browserProxy.setSearchEnginesInfo(generateSearchEngineInfo());
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-search-page');
    document.body.appendChild(page);
    await microtasksFinished();
  });

  teardown(function() {
    page.remove();
  });

  // Tests that the page is querying and displaying search engine info on
  // startup.
  test('Initialization', async function() {
    await browserProxy.whenCalled('getSearchEnginesList');
    await microtasksFinished();

    // Open the search engine list dialog.
    const openSearchEngineListButton =
        page.shadowRoot.querySelector<HTMLButtonElement>('#openDialogButton')!;
    openSearchEngineListButton.click();
    assertEquals(metrics.count('ChooseDefaultSearchEngine'), 1);

    await microtasksFinished();

    const searchEngineListDialog =
        page.shadowRoot.querySelector('settings-search-engine-list-dialog');
    assertTrue(!!searchEngineListDialog);

    const radioGroupElement =
        searchEngineListDialog.shadowRoot.querySelector('cr-radio-group')!;
    assertEquals('db:0', radioGroupElement.selected);

    const saveGuestChoiceCheckbox =
        searchEngineListDialog.shadowRoot.querySelector(
            '#saveGuestChoiceCheckbox');
    assertFalse(!!saveGuestChoiceCheckbox);

    // Simulate a user initiated change of the default search engine.
    const radioButtons =
        searchEngineListDialog.shadowRoot.querySelectorAll('cr-radio-button');
    const setAsDefaultButton =
        searchEngineListDialog.shadowRoot.querySelector<HTMLButtonElement>(
            '#setAsDefaultButton')!;
    radioButtons[1]!.click();
    await microtasksFinished();
    setAsDefaultButton.click();

    const [, , saveGuestChoice] =
        await browserProxy.whenCalled('setDefaultSearchEngine');
    assertEquals(saveGuestChoice, null);

    assertEquals('db:1', radioGroupElement.selected);

    // Simulate a change that happened in a different tab.
    const searchEnginesInfo = generateSearchEngineInfo();
    searchEnginesInfo.defaults[0]!.default = false;
    searchEnginesInfo.defaults[1]!.default = false;
    searchEnginesInfo.defaults[2]!.default = true;

    browserProxy.resetResolver('setDefaultSearchEngine');
    webUIListenerCallback('search-engines-changed', searchEnginesInfo);
    await microtasksFinished();
    assertEquals('db:2', radioGroupElement.selected);

    browserProxy.whenCalled('setDefaultSearchEngine').then(function() {
      // Since the change happened in a different tab, there should be
      // no new call to |setDefaultSearchEngine|.
      assertNotReached('Should not call setDefaultSearchEngine again');
    });
  });

  test('ControlledByExtension', async function() {
    await browserProxy.whenCalled('getSearchEnginesList');

    const openSearchEngineListButton =
        page.shadowRoot.querySelector<HTMLButtonElement>('#openDialogButton')!;
    assertFalse(openSearchEngineListButton.disabled);
    assertFalse(
        !!page.shadowRoot.querySelector('extension-controlled-indicator'));

    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'default_search_provider_data.template_url_data',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {},
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
      controlledByName: 'fake extension name',
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      extensionId: 'fake extension id',
      extensionCanBeDisabled: true,
    }]);
    await microtasksFinished();

    assertTrue(openSearchEngineListButton['disabled']);
    assertTrue(
        !!page.shadowRoot.querySelector('extension-controlled-indicator'));
    assertFalse(!!page.shadowRoot.querySelector('cr-policy-pref-indicator'));

    // The extension controlled message is not shown.
    assertFalse(
        !!page.shadowRoot.querySelector('extension-controlled-message'));
  });

  test('ControlledByPolicy', async function() {
    await browserProxy.whenCalled('getSearchEnginesList');
    const openSearchEngineListButton =
        page.shadowRoot.querySelector<HTMLButtonElement>('#openDialogButton')!;
    assertFalse(openSearchEngineListButton.disabled);
    assertFalse(
        !!page.shadowRoot.querySelector('extension-controlled-indicator'));

    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'default_search_provider_data.template_url_data',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {},
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    }]);
    await microtasksFinished();

    assertTrue(openSearchEngineListButton.disabled);
    assertFalse(
        !!page.shadowRoot.querySelector('extension-controlled-indicator'));
    assertTrue(!!page.shadowRoot.querySelector('cr-policy-pref-indicator'));
  });

  test('ShowGuestSaveCheckbox', async function() {
    browserProxy.setSaveGuestChoice(true);
    await browserProxy.whenCalled('getSearchEnginesList');
    await microtasksFinished();

    // Open the search engine list dialog.
    const openSearchEngineListButton =
        page.shadowRoot.querySelector<HTMLButtonElement>('#openDialogButton')!;
    openSearchEngineListButton.click();
    assertEquals(metrics.count('ChooseDefaultSearchEngine'), 1);

    await microtasksFinished();

    const searchEngineListDialog =
        page.shadowRoot.querySelector('settings-search-engine-list-dialog');
    assertTrue(!!searchEngineListDialog);

    const saveGuestChoiceCheckbox =
        searchEngineListDialog.shadowRoot.querySelector<CrCheckboxElement>(
            '#saveGuestChoiceCheckbox')!;
    assertTrue(!!saveGuestChoiceCheckbox);
    assertTrue(saveGuestChoiceCheckbox.checked);

    saveGuestChoiceCheckbox.click();
    await microtasksFinished();
    assertFalse(saveGuestChoiceCheckbox.checked);

    const setAsDefaultButton =
        searchEngineListDialog.shadowRoot.querySelector<HTMLButtonElement>(
            '#setAsDefaultButton')!;
    setAsDefaultButton.click();

    const [, , saveGuestChoice] =
        await browserProxy.whenCalled('setDefaultSearchEngine');
    assertFalse(saveGuestChoice);
  });

  test('Link row navigates to search engines subpage', async function() {
    await microtasksFinished();
    const trigger =
        page.shadowRoot.querySelector<HTMLElement>('#enginesSubpageTrigger');
    assertTrue(!!trigger);

    trigger.click();
    assertEquals(routes.SEARCH_ENGINES, Router.getInstance().getCurrentRoute());
    const interaction =
        await browserProxy.whenCalled('recordSearchEnginesPageHistogram');
    assertEquals(SearchEnginesInteractions.SUBPAGE_NAVIGATED, interaction);
  });

  test(
      'Fetches picker data when SearchSettingsWithMoreEngines is enabled',
      async function() {
        page.remove();
        loadTimeData.overrideValues({
          searchSettingsUpdate: false,
          pickerWithMoreEngines: true,
        });
        browserProxy.setDefaultSearchEnginePickerData(
            generateDefaultSearchEnginePickerData());

        page = document.createElement('settings-search-page');
        document.body.appendChild(page);
        await browserProxy.whenCalled('getDefaultSearchEnginePickerData');
        await microtasksFinished();

        const openSearchEngineListButton =
            page.shadowRoot.querySelector<HTMLButtonElement>(
                '#openDialogButton')!;
        openSearchEngineListButton.click();
        await microtasksFinished();

        const searchEngineListDialog =
            page.shadowRoot.querySelector('settings-search-engine-list-dialog');
        assertTrue(!!searchEngineListDialog);
        assertDeepEquals(
            generateDefaultSearchEnginePickerData().primary,
            searchEngineListDialog.searchEngines);
      });

  /**
   * Recreates the page with the "More options" entry point enabled, and
   * replaces the search engine list dialog with the more engines dialog.
   * `beforeSwitch` is called with the list dialog before switching.
   */
  async function openMoreEnginesDialog(
      beforeSwitch?: (listDialog: SettingsSearchEngineListDialogElement) =>
          Promise<void>):
      Promise<SettingsSearchEngineMoreEnginesDialogElement> {
    page.remove();
    loadTimeData.overrideValues({
      searchSettingsUpdate: false,
      pickerWithMoreEngines: true,
    });
    browserProxy.setDefaultSearchEnginePickerData(
        generateDefaultSearchEnginePickerData());
    browserProxy.setMoreSearchEngines([createSampleSearchEngine({
      name: 'DuckDuckGo',
      id: engineId('db:10'),
      canBeDefault: true,
    })]);
    page = document.createElement('settings-search-page');
    document.body.appendChild(page);
    await browserProxy.whenCalled('getDefaultSearchEnginePickerData');
    await microtasksFinished();

    page.shadowRoot.querySelector<HTMLElement>('#openDialogButton')!.click();
    await microtasksFinished();
    const searchEngineListDialog =
        page.shadowRoot.querySelector('settings-search-engine-list-dialog');
    assertTrue(!!searchEngineListDialog);
    if (beforeSwitch) {
      await beforeSwitch(searchEngineListDialog);
    }

    const moreEnginesButton =
        searchEngineListDialog.shadowRoot.querySelector<HTMLElement>(
            '#moreEnginesButton');
    assertTrue(!!moreEnginesButton);
    moreEnginesButton.click();
    await microtasksFinished();

    const moreEnginesDialog = page.shadowRoot.querySelector(
        'settings-search-engine-more-engines-dialog');
    assertTrue(!!moreEnginesDialog);
    return moreEnginesDialog;
  }

  test(
      'More options replaces the list dialog with the more engines dialog',
      async function() {
        browserProxy.setSaveGuestChoice(true);
        const moreEnginesDialog = await openMoreEnginesDialog(
            async (listDialog: SettingsSearchEngineListDialogElement) => {
              // Uncheck the guest choice checkbox of the list dialog.
              const checkbox =
                  listDialog.shadowRoot.querySelector<CrCheckboxElement>(
                      '#saveGuestChoiceCheckbox');
              assertTrue(!!checkbox);
              assertTrue(checkbox.checked);
              checkbox.click();
              await microtasksFinished();
            });

        // The dialogs are not stacked.
        assertFalse(!!page.shadowRoot.querySelector(
            'settings-search-engine-list-dialog'));

        // The more engines dialog is a modal dialog of its own, with the
        // search field focused by the browser.
        assertTrue(moreEnginesDialog.$.dialog.getNative().matches(':modal'));
        assertEquals(
            moreEnginesDialog.$.searchField.getSearchInput().inputElement,
            getDeepActiveElement());

        // The guest choice of the list dialog is carried over.
        const checkbox =
            moreEnginesDialog.shadowRoot.querySelector<CrCheckboxElement>(
                '#saveGuestChoiceCheckbox');
        assertTrue(!!checkbox);
        assertFalse(checkbox.checked);
      });

  test(
      'Cancelling the more engines dialog returns the focus to the button',
      async function() {
        const moreEnginesDialog = await openMoreEnginesDialog();

        const closed = eventToPromise('close', moreEnginesDialog);
        moreEnginesDialog.shadowRoot
            .querySelector<HTMLElement>('#cancelButton')!.click();
        await closed;
        await microtasksFinished();

        assertFalse(!!page.shadowRoot.querySelector(
            'settings-search-engine-more-engines-dialog'));
        assertEquals(
            page.shadowRoot.querySelector('#openDialogButton'),
            getDeepActiveElement());
      });

  test(
      'Choosing an engine in the more engines dialog shows the toast',
      async function() {
        const moreEnginesDialog = await openMoreEnginesDialog();

        moreEnginesDialog.shadowRoot.querySelector('cr-radio-button')!.click();
        await microtasksFinished();
        const closed = eventToPromise('close', moreEnginesDialog);
        moreEnginesDialog.shadowRoot
            .querySelector<HTMLElement>('#setAsDefaultButton')!.click();
        await closed;
        await microtasksFinished();

        assertFalse(!!page.shadowRoot.querySelector(
            'settings-search-engine-more-engines-dialog'));
        const confirmationToast =
            page.shadowRoot.querySelector<CrToastElement>('#confirmationToast');
        assertTrue(!!confirmationToast);
        assertTrue(confirmationToast.open);
      });
});

suite('SearchPageWithSearchSettingsUpdateEnabledTests', function() {
  let page: SettingsSearchPageElement;
  let browserProxy: TestSearchEnginesBrowserProxy;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let prefService: PrefService;
  let metrics: MetricsTracker;

  setup(async function() {
    loadTimeData.overrideValues({
      searchSettingsUpdate: true,
      pickerWithMoreEngines: false,
    });
    resetRouterForTesting();

    prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    metrics = fakeMetricsPrivate();
    browserProxy = new TestSearchEnginesBrowserProxy();
    browserProxy.setDefaultSearchEnginePickerData(
        generateDefaultSearchEnginePickerData());
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-search-page');
    document.body.appendChild(page);

    await browserProxy.whenCalled('getDefaultSearchEnginePickerData');
    await microtasksFinished();
  });

  test('Link row to search engines subpage is not visible', function() {
    const trigger =
        page.shadowRoot.querySelector<HTMLElement>('#enginesSubpageTrigger');
    assertFalse(!!trigger);
  });

  test(
      'Primary search engines passed to search engine list dialog',
      async function() {
        // Open the search engine list dialog.
        const openSearchEngineListButton =
            page.shadowRoot.querySelector<HTMLButtonElement>(
                '#openDialogButton');
        assertTrue(!!openSearchEngineListButton);
        openSearchEngineListButton.click();
        assertEquals(1, metrics.count('ChooseDefaultSearchEngine'));
        await microtasksFinished();

        const searchEngineListDialog =
            page.shadowRoot.querySelector('settings-search-engine-list-dialog');
        assertTrue(!!searchEngineListDialog);

        assertDeepEquals(
            generateDefaultSearchEnginePickerData().primary,
            searchEngineListDialog.searchEngines);

        // Simulate a change in search engines.
        const updatedPickerData = generateDefaultSearchEnginePickerData();
        updatedPickerData.primary[0]!.default = false;
        updatedPickerData.primary[2]!.default = true;
        browserProxy.setDefaultSearchEnginePickerData(updatedPickerData);
        browserProxy.resetResolver('getDefaultSearchEnginePickerData');
        webUIListenerCallback('search-engines-changed');
        await browserProxy.whenCalled('getDefaultSearchEnginePickerData');
        await microtasksFinished();

        assertDeepEquals(
            updatedPickerData.primary, searchEngineListDialog.searchEngines);
      });

  test('ControlledByExtension', async function() {
    const openSearchEngineListButton =
        page.shadowRoot.querySelector<HTMLButtonElement>('#openDialogButton')!;
    assertFalse(openSearchEngineListButton.disabled);
    assertFalse(
        !!page.shadowRoot.querySelector('extension-controlled-message'));

    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'default_search_provider_data.template_url_data',
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
      controlledByName: 'fake extension name',
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      extensionId: 'fake extension id',
      extensionCanBeDisabled: true,
    }]);
    await microtasksFinished();

    assertTrue(openSearchEngineListButton['disabled']);
    assertTrue(!!page.shadowRoot.querySelector('extension-controlled-message'));
    assertFalse(!!page.shadowRoot.querySelector('cr-policy-pref-indicator'));

    // The extension controlled indicator is not shown.
    assertFalse(
        !!page.shadowRoot.querySelector('extension-controlled-indicator'));
  });
});
