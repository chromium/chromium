// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsOmniboxExtensionEntryElement, SettingsSearchEngineEntryElement} from 'chrome://settings/lazy_load.js';
import type {SearchEngine} from 'chrome://settings/settings.js';
import {ExtensionControlBrowserProxyImpl, SearchEnginesBrowserProxyImpl, ChoiceMadeLocation} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestExtensionControlBrowserProxy} from './test_extension_control_browser_proxy.js';
import {createSampleOmniboxExtension, createSampleSearchEngine, TestSearchEnginesBrowserProxy} from './test_search_engines_browser_proxy.js';
// clang-format on

suite('SearchEngineEntryTest', function() {
  let entry: SettingsSearchEngineEntryElement;
  let browserProxy: TestSearchEnginesBrowserProxy;

  const searchEngine = createSampleSearchEngine(
      {canBeDefault: true, canBeEdited: true, canBeRemoved: true});

  setup(function() {
    browserProxy = new TestSearchEnginesBrowserProxy();
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    entry = document.createElement('settings-search-engine-entry');
    entry.engine = searchEngine;
    document.body.appendChild(entry);
  });

  // Test that the <search-engine-entry> is populated according to its
  // underlying SearchEngine model.
  test('Initialization', function() {
    flush();
    assertEquals(
        searchEngine.displayName,
        entry.shadowRoot!.querySelector('#name-column')!.textContent!.trim());
    assertEquals(
        searchEngine.keyword,
        entry.shadowRoot!.querySelector('#shortcut-column')!.textContent);
    assertEquals(
        searchEngine.url,
        entry.shadowRoot!.querySelector('#url-column')!.textContent);
  });

  // Tests that columns are hidden and shown appropriately.
  test('ColumnVisibility', function() {
    flush();

    // Test shortcut column visibility.
    entry.showShortcut = true;
    assertFalse(
        entry.shadowRoot!.querySelector<HTMLElement>(
                             '#shortcut-column')!.hidden);
    entry.showShortcut = false;
    assertTrue(entry.shadowRoot!.querySelector<HTMLElement>(
                                    '#shortcut-column')!.hidden);

    // Test query URL column visibility.
    entry.showQueryUrl = true;
    assertFalse(
        entry.shadowRoot!.querySelector<HTMLElement>('#url-column')!.hidden);
    entry.showQueryUrl = false;
    assertTrue(
        entry.shadowRoot!.querySelector<HTMLElement>('#url-column')!.hidden);
  });

  // Open and return the action menu
  function openActionMenu() {
    const menuButton = entry.shadowRoot!.querySelector<HTMLElement>(
        'cr-icon-button.icon-more-vert');
    assertTrue(!!menuButton);
    menuButton!.click();
    const menu = entry.shadowRoot!.querySelector('cr-action-menu')!;
    assertTrue(menu.open);
    return menu;
  }

  test('Remove_Enabled', async function() {
    const menu = openActionMenu();

    const deleteButton = entry.$.delete;
    assertTrue(!!deleteButton);
    assertFalse(deleteButton.hidden);
    deleteButton.click();
    const modelIndex = await browserProxy.whenCalled('removeSearchEngine');
    assertFalse(menu.open);
    assertEquals(entry.engine.modelIndex, modelIndex);
  });

  test('MakeDefault_Enabled', async function() {
    const menu = openActionMenu();

    const makeDefaultButton = entry.$.makeDefault;
    assertTrue(!!makeDefaultButton);
    makeDefaultButton.click();
    const [modelIndex, choiceMadeLocation] =
        await browserProxy.whenCalled('setDefaultSearchEngine');
    assertEquals(choiceMadeLocation, ChoiceMadeLocation.SEARCH_ENGINE_SETTINGS);
    assertFalse(menu.open);
    assertEquals(entry.engine.modelIndex, modelIndex);
  });

  // Test that clicking the "edit" menu item fires an edit event.
  test('Edit_Enabled', function() {
    flush();

    const engine = entry.engine;
    const editButton =
        entry.shadowRoot!.querySelector<HTMLButtonElement>(`#editIconButton`)!;
    assertTrue(!!editButton);
    assertFalse(editButton.hidden);

    const promise =
        eventToPromise('view-or-edit-search-engine', entry).then(e => {
          assertEquals(engine, e.detail.engine);
          assertEquals(
              entry.shadowRoot!.querySelector('cr-icon-button'),
              e.detail.anchorElement);
        });
    editButton.click();
    return promise;
  });

  /**
   * Checks that the given button is hidden for the given search engine.
   */
  function testButtonHidden(searchEngine: SearchEngine, buttonId: string) {
    entry.engine = searchEngine;
    const button =
        entry.shadowRoot!.querySelector<HTMLButtonElement>(`#${buttonId}`);
    assertTrue(!!button);
    assertTrue(button!.hidden);
  }

  test('Remove_Hidden', function() {
    testButtonHidden(createSampleSearchEngine({canBeRemoved: false}), 'delete');
  });

  test('Activate_Hidden', function() {
    flush();
    testButtonHidden(
        createSampleSearchEngine({canBeActivated: false}), 'activate');
  });

  test('Deactivate_Hidden', function() {
    flush();
    testButtonHidden(
        createSampleSearchEngine({canBeDeactivated: false}), 'deactivate');
  });

  test('Edit_Hidden', function() {
    flush();
    testButtonHidden(
        createSampleSearchEngine({canBeActivated: true}), 'editIconButton');

    flush();
    testButtonHidden(
        createSampleSearchEngine({isStarterPack: true}), 'editIconButton');
  });
  /**
   * Checks that the given button is disabled for the given search engine.
   */
  function testButtonDisabled(searchEngine: SearchEngine, buttonId: string) {
    entry.engine = searchEngine;
    const button =
        entry.shadowRoot!.querySelector<HTMLButtonElement>(`#${buttonId}`);
    assertTrue(!!button);
    assertTrue(button!.disabled);
  }

  test('MakeDefault_Disabled', function() {
    testButtonDisabled(
        createSampleSearchEngine({canBeDefault: false}), 'makeDefault');
  });

  test('Edit_Disabled', function() {
    flush();
    testButtonDisabled(
        createSampleSearchEngine({canBeEdited: false}), 'editIconButton');
  });

  // Test that clicking the "activate" button fires an activate event.
  test('Activate', async function() {
    flush();
    entry.engine = createSampleSearchEngine({canBeActivated: true});

    const activateButton = entry.shadowRoot!.querySelector<HTMLButtonElement>(
        'cr-button.secondary-button')!;
    assertTrue(!!activateButton);
    assertFalse(activateButton.hidden);
    activateButton.click();

    // Ensure that the activate event is fired.
    const [modelIndex, isActive] =
        await browserProxy.whenCalled('setIsActiveSearchEngine');
    assertEquals(entry.engine.modelIndex, modelIndex);
    assertTrue(isActive);
  });

  // Test that clicking the "Deactivate" button fires a deactivate event.
  test('Deactivate', async function() {
    flush();
    entry.engine = createSampleSearchEngine({canBeDeactivated: true});

    // Open action menu.
    entry.shadowRoot!
        .querySelector<HTMLElement>('cr-icon-button.icon-more-vert')!.click();
    const menu = entry.shadowRoot!.querySelector('cr-action-menu')!;
    assertTrue(menu.open);

    const deactivateButton = entry.shadowRoot!.querySelector<HTMLButtonElement>(
        'button#deactivate.dropdown-item')!;
    assertTrue(!!deactivateButton);
    assertFalse(deactivateButton.hidden);
    deactivateButton.click();

    // Ensure that the deactivate event is fired.
    const [modelIndex, isActive] =
        await browserProxy.whenCalled('setIsActiveSearchEngine');
    assertEquals(entry.engine.modelIndex, modelIndex);
    assertFalse(isActive);
  });

  // Test that the accessibility Aria labels are set correctly for the Edit,
  // Activate, and More Actions buttons.
  test('AriaLabelSetCorrectly', function() {
    flush();
    entry.engine = createSampleSearchEngine(
        {default: false, canBeActivated: true, canBeEdited: true});

    // Edit button
    const editButton =
        entry.shadowRoot!.querySelector<HTMLElement>('#editIconButton');
    assertTrue(!!editButton);
    assertEquals(
        entry.i18n(
            'searchEnginesEditButtonAriaLabel', entry.engine.displayName),
        editButton.ariaLabel);

    // Activate button
    const activateButton =
        entry.shadowRoot!.querySelector<HTMLElement>('#activate');
    assertTrue(!!activateButton);
    assertEquals(
        entry.i18n(
            'searchEnginesActivateButtonAriaLabel', entry.engine.displayName),
        activateButton.ariaLabel);

    // More actions button
    const menuButton = entry.shadowRoot!.querySelector<HTMLElement>(
        'cr-icon-button.icon-more-vert');
    assertTrue(!!menuButton);
    assertEquals(
        entry.i18n(
            'searchEnginesMoreActionsAriaLabel', entry.engine.displayName),
        menuButton.ariaLabel);
  });
});

suite('OmniboxExtensionEntryTest', function() {
  let entry: SettingsOmniboxExtensionEntryElement;
  let browserProxy: TestExtensionControlBrowserProxy;

  setup(function() {
    browserProxy = new TestExtensionControlBrowserProxy();
    ExtensionControlBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    entry = document.createElement('settings-omnibox-extension-entry');
    entry.engine = createSampleOmniboxExtension();
    document.body.appendChild(entry);

    // Open action menu.
    entry.shadowRoot!.querySelector('cr-icon-button')!.click();
  });

  test('Manage', async function() {
    const manageButton = entry.$.manage;
    assertTrue(!!manageButton);
    manageButton.click();
    const extensionId = await browserProxy.whenCalled('manageExtension');
    assertEquals(entry.engine.extension!.id, extensionId);
  });

  test('Disable', async function() {
    const disableButton = entry.$.disable;
    assertTrue(!!disableButton);
    disableButton.click();
    const extensionId = await browserProxy.whenCalled('disableExtension');
    assertEquals(entry.engine.extension!.id, extensionId);
  });
});

suite('EnterpriseSiteSearchEntryTests', function() {
  let entry: SettingsSearchEngineEntryElement;
  let browserProxy: TestSearchEnginesBrowserProxy;

  const searchEngine = createSampleSearchEngine({
    id: 1,
    name: 'managed',
    canBeEdited: false,
    displayName: 'Managed',
    isManaged: true,
  });

  setup(function() {
    browserProxy = new TestSearchEnginesBrowserProxy();
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    entry = document.createElement('settings-search-engine-entry');
    entry.engine = searchEngine;
    document.body.appendChild(entry);
  });

  // Test that the <search-engine-entry> is populated according to its
  // underlying SearchEngine model.
  test('Initialization', function() {
    flush();
    assertEquals(
        searchEngine.displayName,
        entry.shadowRoot!.querySelector('#name-column')!.textContent!.trim());
    assertEquals(
        searchEngine.keyword,
        entry.shadowRoot!.querySelector('#shortcut-column')!.textContent);
    assertEquals(
        searchEngine.url,
        entry.shadowRoot!.querySelector('#url-column')!.textContent);
  });

  // Verifies that the "edit" and "activate" buttons and the 3-dot action menu
  // are hidden.
  test('BasicControlsHidden', function() {
    flush();

    const editButton =
        entry.shadowRoot!.querySelector<HTMLButtonElement>(`#editIconButton`);
    assertTrue(!!editButton);
    assertTrue(editButton.hidden);

    const activateButton =
        entry.shadowRoot!.querySelector<HTMLButtonElement>(`#activate`);
    assertTrue(!!activateButton);
    assertTrue(activateButton.hidden);

    const menuButton = entry.shadowRoot!.querySelector<HTMLElement>(
        'cr-icon-button.icon-more-vert');
    assertTrue(!!menuButton);
    assertTrue(menuButton.hidden);
  });

  // Verifies that the details can be seen.
  test('ViewDetailsAllowed', function() {
    flush();

    const engine = entry.engine;
    const viewDetailsButton =
        entry.shadowRoot!.querySelector<HTMLButtonElement>(
            `#viewDetailsButton`);
    assertTrue(!!viewDetailsButton);
    assertFalse(viewDetailsButton.hidden);

    const promise =
        eventToPromise('view-or-edit-search-engine', entry).then(e => {
          assertEquals(engine, e.detail.engine);
          assertEquals(
              entry.shadowRoot!.querySelector('cr-icon-button'),
              e.detail.anchorElement);
        });
    viewDetailsButton.click();
    return promise;
  });

  // Verifies that the policy indicator is shown.
  test('PolicyIndicator_Shown', function() {
    flush();

    const policyIndicator =
        entry.shadowRoot!.querySelector('cr-policy-indicator');
    assertTrue(!!policyIndicator);
    assertFalse(policyIndicator.hidden);
  });
});
