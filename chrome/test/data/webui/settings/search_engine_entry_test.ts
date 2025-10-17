// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsOmniboxExtensionEntryElement, SettingsSearchEngineEntryElement} from 'chrome://settings/lazy_load.js';
import type { SearchEngine, CrActionMenuElement } from 'chrome://settings/settings.js';
import { ExtensionControlBrowserProxyImpl, SearchEnginesBrowserProxyImpl, ChoiceMadeLocation } from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import { eventToPromise, isVisible } from 'chrome://webui-test/test_util.js';

import {TestExtensionControlBrowserProxy} from './test_extension_control_browser_proxy.js';
import {createSampleOmniboxExtension, createSampleSearchEngine, TestSearchEnginesBrowserProxy} from './test_search_engines_browser_proxy.js';
// clang-format on

/**
 * Opens and returns the action menu for a SettingsSearchEngineEntryElement.
 */
function openActionMenu(entry: SettingsSearchEngineEntryElement):
    CrActionMenuElement {
  const menuButton = entry.shadowRoot!.querySelector<HTMLElement>(
      'cr-icon-button.icon-more-vert');
  assertTrue(!!menuButton);
  menuButton.click();
  const menu = entry.shadowRoot!.querySelector('cr-action-menu');
  assertTrue(!!menu);
  assertTrue(menu.open);
  return menu;
}

/**
 * Checks that the given button is hidden.
 */
function assertButtonHidden(
    entry: SettingsSearchEngineEntryElement, buttonId: string,
    searchEngine?: SearchEngine) {
  if (searchEngine) {
    entry.engine = searchEngine;
  }
  const button = entry.shadowRoot!.querySelector<HTMLButtonElement>(buttonId);
  assertTrue(!!button);
  assertTrue(button.hidden);
}

/**
 * Returns whether the given button is disabled.
 */
function isButtonDisabled(
    entry: SettingsSearchEngineEntryElement, buttonId: string,
    searchEngine?: SearchEngine): boolean {
  if (searchEngine) {
    entry.engine = searchEngine;
  }
  const button = entry.shadowRoot!.querySelector<HTMLButtonElement>(buttonId);
  assertTrue(!!button);
  return button.disabled;
}

suite('SearchEngineEntryTest', function() {
  let entry: SettingsSearchEngineEntryElement;
  let browserProxy: TestSearchEnginesBrowserProxy;

  const searchEngine = createSampleSearchEngine(
      {canBeDefault: true, canBeEdited: true, canBeRemoved: true});

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestSearchEnginesBrowserProxy();
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);
    entry = document.createElement('settings-search-engine-entry');
    entry.engine = searchEngine;
    document.body.appendChild(entry);
  });

  // Test that the <search-engine-entry> is populated according to its
  // underlying SearchEngine model.
  test('Initialization', function() {
    assertEquals(
        searchEngine.displayName,
        entry.shadowRoot!.querySelector('#name-column')!.textContent.trim());
    assertEquals(
        searchEngine.keyword,
        entry.shadowRoot!.querySelector('#shortcut-column')!.textContent);
    assertEquals(
        searchEngine.url,
        entry.shadowRoot!.querySelector('#url-column')!.textContent);
  });

  // Tests that columns are hidden and shown appropriately.
  test('ColumnVisibility', function() {
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

  test('Remove_Enabled', async function() {
    const menu = openActionMenu(entry);

    const deleteButton = entry.$.delete;
    assertTrue(isVisible(deleteButton));
    deleteButton.click();
    const modelIndex = await browserProxy.whenCalled('removeSearchEngine');
    assertFalse(menu.open);
    assertEquals(entry.engine.modelIndex, modelIndex);
  });

  test('MakeDefault_Enabled', async function() {
    const menu = openActionMenu(entry);

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
    const engine = entry.engine;
    const editButton =
        entry.shadowRoot!.querySelector<HTMLButtonElement>(`#editIconButton`)!;
    assertTrue(isVisible(editButton));

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

  test('Remove_Hidden', function() {
    assertButtonHidden(
        entry, '#delete', createSampleSearchEngine({canBeRemoved: false}));
  });

  test('Activate_Hidden', function() {
    assertButtonHidden(
        entry, '#activate', createSampleSearchEngine({canBeActivated: false}));
  });

  test('Deactivate_Hidden', function() {
    assertButtonHidden(
        entry, '#deactivate',
        createSampleSearchEngine({canBeDeactivated: false}));
  });

  test('Edit_Hidden', function() {
    assertButtonHidden(
        entry, '#editIconButton',
        createSampleSearchEngine({canBeActivated: true}));

    assertButtonHidden(
        entry, '#editIconButton',
        createSampleSearchEngine({isStarterPack: true}));
  });

  test('MakeDefault_Disabled', function() {
    assertTrue(isButtonDisabled(
        entry, '#makeDefault',
        createSampleSearchEngine({canBeDefault: false})));
  });

  test('Edit_Disabled', function() {
    assertTrue(isButtonDisabled(
        entry, '#editIconButton',
        createSampleSearchEngine({canBeEdited: false})));
  });

  // Test that clicking the "activate" button fires an activate event.
  test('Activate', async function() {
    entry.engine = createSampleSearchEngine({canBeActivated: true});

    const activateButton = entry.shadowRoot!.querySelector<HTMLButtonElement>(
        'cr-button.secondary-button')!;
    assertTrue(isVisible(activateButton));
    activateButton.click();

    // Ensure that the activate event is fired.
    const [modelIndex, isActive] =
        await browserProxy.whenCalled('setIsActiveSearchEngine');
    assertEquals(entry.engine.modelIndex, modelIndex);
    assertTrue(isActive);
  });

  // Test that clicking the "Deactivate" button fires a deactivate event.
  test('Deactivate', async function() {
    entry.engine = createSampleSearchEngine({canBeDeactivated: true});

    // Open action menu.
    entry.shadowRoot!
        .querySelector<HTMLElement>('cr-icon-button.icon-more-vert')!.click();
    const menu = entry.shadowRoot!.querySelector('cr-action-menu')!;
    assertTrue(menu.open);

    const deactivateButton = entry.shadowRoot!.querySelector<HTMLButtonElement>(
        'button#deactivate.dropdown-item')!;
    assertTrue(isVisible(deactivateButton));
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


  // Test that when a search engine has an iconPath, site-favicon displays the
  // icon. Downloaded icon should not be visible.
  test('FaviconWithIconPath', function() {
    entry.engine = createSampleSearchEngine({
      iconPath: 'images/foo.png',
      iconURL: 'http://www.google.com/favicon.ico',
    });

    assertEquals(
        'chrome://image/?http://www.google.com/favicon.ico',
        entry.$.downloadedIcon.src);
    assertFalse(isVisible(entry.$.downloadedIcon));

    const siteFavicon = entry.shadowRoot!.querySelector('site-favicon');
    assertTrue(!!siteFavicon);
    const favicon = siteFavicon.shadowRoot!.querySelector('#favicon');
    assertTrue(!!favicon);
    assertTrue(isVisible(favicon));
  });

  // Test that when a search engine has an iconURL and downloading is
  // successful, the downloaded icon is displayed. The site-favicon should not
  // be visible.
  test('FaviconWithIconURL_Successful', async function() {
    entry.engine = createSampleSearchEngine({
      iconPath: '',
      iconURL: 'chrome://resources/images/chrome_logo_dark.svg',
    });

    await eventToPromise('load', entry.$.downloadedIcon);
    assertEquals(
        'chrome://resources/images/chrome_logo_dark.svg',
        entry.$.downloadedIcon.src);
    assertTrue(isVisible(entry.$.downloadedIcon));

    const siteFavicon = entry.shadowRoot!.querySelector('site-favicon');
    assertTrue(!!siteFavicon);
    const favicon = siteFavicon.shadowRoot!.querySelector('#favicon');
    assertTrue(!!favicon);
    assertFalse(isVisible(favicon));
  });

  // Test that when a search engine has an iconURL and downloading fails,
  // site-favicon displays the icon.
  test('FaviconWithIconURL_Failed', async function() {
    entry.engine = createSampleSearchEngine(
        {iconPath: '', iconURL: 'chrome://resources/images/invalid_url'});

    await eventToPromise('error', entry.$.downloadedIcon);
    assertEquals(
        'chrome://resources/images/invalid_url', entry.$.downloadedIcon.src);
    assertFalse(isVisible(entry.$.downloadedIcon));

    const siteFavicon = entry.shadowRoot!.querySelector('site-favicon');
    assertTrue(!!siteFavicon);
    const favicon = siteFavicon.shadowRoot!.querySelector('#favicon');
    assertTrue(!!favicon);
    assertTrue(isVisible(favicon));
  });

  // Test that when a search engine has neither an iconPath nor an iconURL,
  // site-favicon displays the icon based on the search engine's URL.
  test('FaviconWithURL', function() {
    entry.engine = createSampleSearchEngine({iconPath: '', iconURL: ''});

    assertEquals('', entry.$.downloadedIcon.src);
    assertFalse(isVisible(entry.$.downloadedIcon));

    const siteFavicon = entry.shadowRoot!.querySelector('site-favicon');
    assertTrue(!!siteFavicon);
    const favicon = siteFavicon.shadowRoot!.querySelector('#favicon')!;
    assertTrue(!!favicon);
    assertTrue(isVisible(favicon));
  });
});

suite('OmniboxExtensionEntryTest', function() {
  let entry: SettingsOmniboxExtensionEntryElement;
  let browserProxy: TestExtensionControlBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestExtensionControlBrowserProxy();
    ExtensionControlBrowserProxyImpl.setInstance(browserProxy);
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

  /**
   * Creates a sample managed search engine.
   */
  const createSampleManagedSearchEngine = (): SearchEngine => {
    return createSampleSearchEngine({
      id: 1,
      name: 'managed',
      canBeEdited: false,
      displayName: 'Managed',
      isManaged: true,
    });
  };

  /**
   * Creates a sample overridable search engine.
   */
  const createSampleOverridableSearchEngine =
      (isFeatured: boolean): SearchEngine => {
        return createSampleSearchEngine({
          id: 1,
          name: 'recommended',
          canBeEdited: !isFeatured,
          canBeRemoved: !isFeatured,
          canBeDeactivated: true,
          displayName: 'Recommended',
          isManaged: true,
          shouldConfirmDeletion: true,
        });
      };

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestSearchEnginesBrowserProxy();
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);
    entry = document.createElement('settings-search-engine-entry');
    document.body.appendChild(entry);
  });

  // Test that the <search-engine-entry> is populated according to its
  // underlying SearchEngine model.
  test('Initialization', function() {
    const assertSiteSearchInitialization =
        (entry: SettingsSearchEngineEntryElement,
         searchEngine: SearchEngine) => {
          assertEquals(
              searchEngine.displayName,
              entry.shadowRoot!.querySelector(
                                   '#name-column')!.textContent.trim());
          assertEquals(
              searchEngine.keyword,
              entry.shadowRoot!.querySelector('#shortcut-column')!.textContent);
          assertEquals(
              searchEngine.url,
              entry.shadowRoot!.querySelector('#url-column')!.textContent);
        };

    // Test for managed engine.
    const managedEngine = createSampleManagedSearchEngine();
    entry.engine = managedEngine;
    assertSiteSearchInitialization(entry, managedEngine);

    // Test for overridable engine (featured).
    const featuredOverridableEngine =
        createSampleOverridableSearchEngine(/*isFeatured=*/ true);
    entry.engine = featuredOverridableEngine;
    assertSiteSearchInitialization(entry, featuredOverridableEngine);

    // Test for overridable engine (unfeatured).
    const unfeaturedOverridableEngine =
        createSampleOverridableSearchEngine(/*isFeatured=*/ false);
    entry.engine = unfeaturedOverridableEngine;
    assertSiteSearchInitialization(entry, unfeaturedOverridableEngine);
  });

  // Verifies that the "Activate" button is hidden for all managed engines.
  test('ActivateButtonBehavior', function() {
    entry.engine = createSampleManagedSearchEngine();
    assertButtonHidden(entry, '#activate');
    entry.engine = createSampleOverridableSearchEngine(/*isFeatured=*/ true);
    assertButtonHidden(entry, '#activate');
    entry.engine = createSampleOverridableSearchEngine(/*isFeatured=*/ false);
    assertButtonHidden(entry, '#activate');
  });

  // Verifies the visibility and functionality of the "edit" button for managed
  // (hidden) and overridable (featured: hidden; unfeatured: visible and fires
  // event) engines.
  test('EditButtonBehavior', async function() {
    // Test for managed engine (Edit button should be hidden).
    entry.engine = createSampleManagedSearchEngine();
    assertButtonHidden(entry, '#editIconButton');

    // Test for featured overridable engine (Edit button should be hidden).
    entry.engine = createSampleOverridableSearchEngine(/*isFeatured=*/ true);
    assertButtonHidden(entry, '#editIconButton');

    // Test for unfeatured overridable engine (Edit button should be visible and
    // functional).
    entry.engine = createSampleOverridableSearchEngine(/*isFeatured=*/ false);
    const engineUnfeatured = entry.engine;
    const editButton =
        entry.shadowRoot!.querySelector<HTMLButtonElement>(`#editIconButton`)!;
    assertTrue(isVisible(editButton));

    const whenFired = eventToPromise('view-or-edit-search-engine', entry);
    editButton.click();
    const e = await whenFired;
    assertEquals(engineUnfeatured, e.detail.engine);
    assertEquals(editButton, e.detail.anchorElement);
  });

  // Verifies that the action menu (three-dot menu) is visible. Non-overridable
  // managed engines should have the menu disabled.
  test('ActionMenuBehavior', function() {
    // Test for managed engine (Menu should be visible and disabled).
    entry.engine = createSampleManagedSearchEngine();
    const menuButton = entry.shadowRoot!.querySelector<HTMLElement>(
        'cr-icon-button.icon-more-vert');
    assertTrue(isVisible(menuButton));
    assertTrue(isButtonDisabled(entry, 'cr-icon-button.icon-more-vert'));

    // Test for featured overridable engine (Action menu should be visible and
    // not disabled).
    entry.engine = createSampleOverridableSearchEngine(/*isFeatured=*/ true);
    const menuButtonFeatured = entry.shadowRoot!.querySelector<HTMLElement>(
        'cr-icon-button.icon-more-vert');
    assertTrue(isVisible(menuButtonFeatured));
    assertFalse(isButtonDisabled(entry, 'cr-icon-button.icon-more-vert'));

    // Test for unfeatured overridable engine (Action menu should be visible and
    // not disabled).
    entry.engine = createSampleOverridableSearchEngine(/*isFeatured=*/ false);
    const menuButtonUnfeatured = entry.shadowRoot!.querySelector<HTMLElement>(
        'cr-icon-button.icon-more-vert');
    assertTrue(isVisible(menuButtonUnfeatured));
    assertFalse(isButtonDisabled(entry, 'cr-icon-button.icon-more-vert'));
  });

  // Verifies that the "Make Default" button is disabled for all overridable
  // engines.
  test('MakeDefaultDisabled_Overridable', function() {
    entry.engine = createSampleOverridableSearchEngine(/*isFeatured=*/ true);
    assertTrue(isButtonDisabled(entry, '#makeDefault'));
    entry.engine = createSampleOverridableSearchEngine(/*isFeatured=*/ false);
    assertTrue(isButtonDisabled(entry, '#makeDefault'));
  });

  // Verifies that clicking the "Deactivate" button in the action menu fires a
  // deactivate event for both featured and unfeatured overridable engines.
  test('DeactivateAllowed_Overridable', async function() {
    const testDeactivation = async (isFeatured: boolean) => {
      entry.engine = createSampleOverridableSearchEngine(isFeatured);
      const menu = openActionMenu(entry);

      const deactivateButton =
          entry.shadowRoot!.querySelector<HTMLButtonElement>(
              'button#deactivate.dropdown-item')!;
      assertTrue(isVisible(deactivateButton));
      deactivateButton.click();

      const [modelIndex, isActive] =
          await browserProxy.whenCalled('setIsActiveSearchEngine');
      assertFalse(menu.open);
      assertEquals(entry.engine.modelIndex, modelIndex);
      assertFalse(isActive);
    };

    await testDeactivation(true);
    await testDeactivation(false);
  });

  // Verifies the visibility and functionality of the "Remove" button for both
  // featured (hidden) and unfeatured (visible and fires event for confirmation
  // dialog) overridable engines.
  test('RemoveButtonBehavior_Overridable', async function() {
    // Test for featured engine (Remove button should be hidden).
    entry.engine = createSampleOverridableSearchEngine(/*isFeatured=*/ true);
    openActionMenu(entry);
    assertButtonHidden(entry, 'button#delete.dropdown-item');

    // Test for unfeatured engine (Remove button should be visible and
    // functional).
    entry.engine = createSampleOverridableSearchEngine(/*isFeatured=*/ false);
    openActionMenu(entry);

    const deleteButton = entry.shadowRoot!.querySelector<HTMLButtonElement>(
        'button#delete.dropdown-item')!;
    assertTrue(isVisible(deleteButton));

    const whenFired = eventToPromise('delete-search-engine', entry);
    deleteButton.click();
    const e = await whenFired;
    assertEquals(entry.engine, e.detail.engine);
    assertEquals(
        entry.shadowRoot!.querySelector('cr-icon-button.icon-more-vert'),
        e.detail.anchorElement);
  });

  // Verifies the visibility and functionality of the "View Details" button for
  // managed (visible and fires event) and overridable (featured: visible and
  // fires event; unfeatured: hidden) engines.
  test('ViewDetailsBehavior', async function() {
    const testViewDetails =
        async (engine: SearchEngine, shouldBeVisible: boolean) => {
      entry.engine = engine;
      if (shouldBeVisible) {
        const managedEngine = entry.engine;
        const viewDetailsButton =
            entry.shadowRoot!.querySelector<HTMLButtonElement>(
                `#viewDetailsButton`)!;
        assertTrue(isVisible(viewDetailsButton));

        const whenFired = eventToPromise('view-or-edit-search-engine', entry);
        viewDetailsButton.click();
        const e = await whenFired;
        assertEquals(managedEngine, e.detail.engine);
        assertEquals(
            entry.shadowRoot!.querySelector('cr-icon-button'),
            e.detail.anchorElement);
      } else {
        assertButtonHidden(entry, '#viewDetailsButton');
      }
    };

    // Test for managed engine ("Details" button should be visible and
    // functional).
    await testViewDetails(
        createSampleManagedSearchEngine(), /*shouldBeVisible=*/ true);

    // Test for unfeatured overridable engine ("Details" button should be
    // hidden).
    await testViewDetails(
        createSampleOverridableSearchEngine(/*isFeatured=*/ false),
        /*shouldBeVisible=*/ false);

    // Test for featured overridable engine ("Details" button should be visible
    // and functional).
    await testViewDetails(
        createSampleOverridableSearchEngine(/*isFeatured=*/ true),
        /*shouldBeVisible=*/ true);
  });

  // Verifies that the policy indicator is shown for all managed engines.
  test('PolicyIndicatorShown', function() {
    const assertSiteSearchPolicyIndicatorShown =
        (entry: SettingsSearchEngineEntryElement) => {
          flush();
          const policyIndicator =
              entry.shadowRoot!.querySelector('cr-policy-indicator');
          assertTrue(isVisible(policyIndicator));
        };

    entry.engine = createSampleManagedSearchEngine();
    assertSiteSearchPolicyIndicatorShown(entry);
    entry.engine = createSampleOverridableSearchEngine(/*isFeatured=*/ true);
    assertSiteSearchPolicyIndicatorShown(entry);
    entry.engine = createSampleOverridableSearchEngine(/*isFeatured=*/ false);
    assertSiteSearchPolicyIndicatorShown(entry);
  });
});
