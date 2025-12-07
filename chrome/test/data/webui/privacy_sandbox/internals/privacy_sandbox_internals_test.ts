// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/expandable_json_viewer.js';
import 'chrome://privacy-sandbox-internals/internals_page.js';
import 'chrome://privacy-sandbox-internals/mojo_timestamp.js';
import 'chrome://privacy-sandbox-internals/mojo_timedelta.js';
import 'chrome://privacy-sandbox-internals/pref_display.js';
import 'chrome://privacy-sandbox-internals/text_copy_button.js';
import 'chrome://privacy-sandbox-internals/value_display.js';

import type {CrFrameListElement} from 'chrome://privacy-sandbox-internals/cr_frame_list.js';
import type {ExpandableJsonViewerElement} from 'chrome://privacy-sandbox-internals/expandable_json_viewer.js';
import type {InternalsPage} from 'chrome://privacy-sandbox-internals/internals_page.js';
import type {PrefDisplayElement} from 'chrome://privacy-sandbox-internals/pref_display.js';
import type {PrivacySandboxInternalsPrefGroup, PrivacySandboxInternalsPrefPageConfig} from 'chrome://privacy-sandbox-internals/pref_page.js';
import type {PrivacySandboxInternalsPref} from 'chrome://privacy-sandbox-internals/privacy_sandbox_internals.mojom-webui.js';
import {PrivacySandboxInternalsBrowserProxy} from 'chrome://privacy-sandbox-internals/privacy_sandbox_internals_browser_proxy.js';
import {Router} from 'chrome://privacy-sandbox-internals/router.js';
import type {TextCopyButton} from 'chrome://privacy-sandbox-internals/text_copy_button.js';
import type {ValueDisplayElement} from 'chrome://privacy-sandbox-internals/value_display.js';
import {defaultLogicalFn, timestampLogicalFn} from 'chrome://privacy-sandbox-internals/value_display.js';
import type {DictionaryValue, ListValue, Value} from 'chrome://resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrivacySandboxInternalsBrowserProxy} from './test_privacy_sandbox_internals_browser_proxy.js';

async function waitForElement(
    root: Element|ShadowRoot, selector: string): Promise<HTMLElement> {
  return new Promise(resolve => {
    const check = () => {
      const element = root.querySelector<HTMLElement>(selector);
      if (element) {
        resolve(element);
      } else {
        requestAnimationFrame(check);
      }
    };
    check();
  });
}

async function waitForCondition(checkFn: () => boolean): Promise<void> {
  return new Promise(resolve => {
    const check = () => {
      if (checkFn()) {
        resolve();
      } else {
        setTimeout(check, 0);
      }
    };
    check();
  });
}

// Test suite for search, filter, and highlight functionality on a Prefs page.
suite('PrivacySandboxInternalsSearchTest', function() {
  let page: InternalsPage;
  let shadowRoot: ShadowRoot;
  let searchInput: HTMLInputElement;

  const MOCK_PREFS: PrivacySandboxInternalsPref[] = [
    {name: 'enable_do_not_track', value: {boolValue: false}},
    {
      name: 'tracking_protection.fingerprinting_protection_enabled',
      value: {boolValue: true},
    },
    {
      name: 'tracking_protection.tracking_protection_onboarding_acked',
      value: {boolValue: false},
    },
    {name: 'profile.cookie_controls_mode', value: {intValue: 2}},
    {name: 'tpcd_experiment.profile_state', value: {intValue: 0}},
  ];

  setup(async function() {
    const browserProxy = new TestPrivacySandboxInternalsBrowserProxy();
    browserProxy.testHandler.setPrefs(MOCK_PREFS);
    PrivacySandboxInternalsBrowserProxy.setInstance(browserProxy);

    Router.resetInstanceForTesting();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('internals-page');
    document.body.appendChild(page);
    await browserProxy.testHandler.whenCalled('readPrefsWithPrefixes');
    await microtasksFinished();

    shadowRoot = page.shadowRoot!;
    const searchBar = await waitForElement(shadowRoot, 'search-bar');
    searchInput =
        (await waitForElement(searchBar.shadowRoot!, '#search-input')) as
        HTMLInputElement;
  });

  async function navigateTo(pageName: string) {
    Router.getInstance().navigateTo(pageName);
    await waitForCondition(() => {
      const params = new URLSearchParams(window.location.search);
      return params.get('page') === pageName;
    });
    await microtasksFinished();
  }

  async function typeInSearch(query: string) {
    searchInput.value = query;
    searchInput.dispatchEvent(new Event('input'));
    await waitForCondition(() => {
      const params = new URLSearchParams(window.location.search);
      return params.get('search') === (query || null);
    });
    await microtasksFinished();
  }

  test('filtersPrefsBySearchTerm', async () => {
    await navigateTo('tracking-protection');
    await typeInSearch('onboarding');

    const tpPanel =
        await waitForElement(shadowRoot, '#tracking-protection-prefs-panel');
    const matchingElement =
        tpPanel.querySelector<PrefDisplayElement>('pref-display:not([hidden])');

    assertTrue(
        !!matchingElement, 'A visible matching element should be found.');
    assertFalse(matchingElement.hidden);

    const nonMatchingElement =
        tpPanel.querySelector<PrefDisplayElement>('pref-display[hidden]');
    assertTrue(
        !!nonMatchingElement, 'A non-matching element should be hidden.');
  });

  test('searchingAndClearingFiltersPrefs', async () => {
    await navigateTo('tracking-protection');

    const tpPanel =
        await waitForElement(shadowRoot, '#tracking-protection-prefs-panel');
    await waitForElement(tpPanel, 'pref-display');
    await typeInSearch('fingerprinting');
    await waitForCondition(() => {
      return tpPanel.querySelectorAll('pref-display:not([hidden])').length ===
          1;
    });
    assertEquals(
        tpPanel.querySelectorAll('pref-display:not([hidden])').length, 1,
        'Search should result in exactly one match.');

    await typeInSearch('');
    await waitForCondition(() => {
      return tpPanel.querySelectorAll('pref-display:not([hidden])').length > 1;
    });
    assertTrue(
        tpPanel.querySelectorAll('pref-display:not([hidden])').length > 1,
        'Clearing search should restore multiple prefs.');
  });

  test('highlightsMatchingTextInPrefs', async () => {
    await navigateTo('tracking-protection');
    const searchTerm = 'fingerprinting';
    await typeInSearch(searchTerm);

    const tpPanel =
        await waitForElement(shadowRoot, '#tracking-protection-prefs-panel');
    const matchingElement =
        await waitForElement(tpPanel, 'pref-display:not([hidden])') as
        PrefDisplayElement;
    const highlightElement = await waitForElement(
        matchingElement.shadowRoot!, '.search-highlight-hit');

    assertTrue(
        !!highlightElement,
        'A <span class="search-highlight-hit"> element should be present for highlights.');
    assertEquals(
        highlightElement.textContent, searchTerm,
        'The highlighted text should match the search term.');
    await typeInSearch('');

    const noHighlightElement =
        matchingElement.shadowRoot!.querySelector('.search-highlight-hit');
    assertFalse(
        !!noHighlightElement,
        'The highlight element should be removed after clearing the search.');
  });
});

// Test suite for the Search Bar UI.
suite('SearchBarUITest', function() {
  let page: InternalsPage;

  setup(async function() {
    const browserProxy = new TestPrivacySandboxInternalsBrowserProxy();
    PrivacySandboxInternalsBrowserProxy.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('internals-page');
    document.body.appendChild(page);
    await microtasksFinished();
  });

  test('search bar is visible', async function() {
    const searchBar = await waitForElement(page.shadowRoot!, 'search-bar');
    assertTrue(!!searchBar, 'Search bar element should be present.');
    assertTrue(
        searchBar.offsetWidth > 0 && searchBar.offsetHeight > 0,
        'Search bar should be visible on the page.');
  });
});

// Test suite for the sidebar toggle functionality.
suite('SidebarToggleTest', function() {
  let page: InternalsPage;
  let frameList: CrFrameListElement;
  let sidebarToggleButton: HTMLElement;

  setup(async function() {
    const browserProxy = new TestPrivacySandboxInternalsBrowserProxy();
    PrivacySandboxInternalsBrowserProxy.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('internals-page');
    document.body.appendChild(page);

    frameList = await waitForElement(page.shadowRoot!, '#ps-page') as
        CrFrameListElement;
    sidebarToggleButton = await waitForElement(
        frameList.shadowRoot!, '#sidebar-visibility-button');
  });

  test('createsSidebarToggleButton', function() {
    assertTrue(
        !!sidebarToggleButton,
        'The sidebar toggle button should be created and found.');
  });

  test('togglesSidebarVisibilityOnClick', async function() {
    const tablist =
        frameList.shadowRoot!.querySelector<HTMLElement>('#tablist');
    assertTrue(!!tablist, 'Sidebar tablist element should exist.');

    assertFalse(
        frameList.hasAttribute('collapsed'),
        'The frame list should not be collapsed initially.');
    assertFalse(
        getComputedStyle(tablist).display === 'none',
        'The tablist should be visible initially.');

    sidebarToggleButton.click();
    await waitForCondition(() => frameList.hasAttribute('collapsed'));

    assertTrue(
        frameList.hasAttribute('collapsed'),
        'The frame list should be collapsed after one click.');
    assertEquals(
        'none', getComputedStyle(tablist).display,
        'The tablist should be hidden when collapsed.');

    sidebarToggleButton.click();
    await waitForCondition(() => !frameList.hasAttribute('collapsed'));

    assertFalse(
        frameList.hasAttribute('collapsed'),
        'The frame list should not be collapsed after a second click.');
    assertFalse(
        getComputedStyle(tablist).display === 'none',
        'The tablist should be visible again.');
  });
});

// Test suite for Sidebar behavior within the live InternalsPage.
suite('PrivacySandboxInternalsFrameListTest', function() {
  let page: InternalsPage;
  let shadowRoot: ShadowRoot;
  let browserProxy: TestPrivacySandboxInternalsBrowserProxy;

  setup(async function() {
    browserProxy = new TestPrivacySandboxInternalsBrowserProxy();
    browserProxy.testHandler.setPrefs([]);
    PrivacySandboxInternalsBrowserProxy.setInstance(browserProxy);
    Router.resetInstanceForTesting();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('internals-page');
    document.body.appendChild(page);
    shadowRoot = page.shadowRoot!;
    await waitForElement(shadowRoot, '[data-page-name="cookies"]');
    await microtasksFinished();
  });

  test('SettingsCategoryHeaderCollapsesAndExpandsOnClick', async () => {
    const prefsHeader = shadowRoot.querySelector<HTMLElement>(
        'div[role="heading"].settings-category-header');
    assertTrue(!!prefsHeader, 'The "Prefs" header should exist.');

    assertFalse(
        prefsHeader.hasAttribute('collapsed'),
        'Prefs header should be expanded initially.');

    prefsHeader.click();
    await microtasksFinished();
    assertTrue(
        prefsHeader.hasAttribute('collapsed'),
        'Prefs header should collapse after click.');
  });

  test('clickingGroupHeaderTogglesCollapseAndHidesSubGroup', async () => {
    const groupHeaders = shadowRoot.querySelectorAll<HTMLElement>(
        'div[role="heading"].settings-category-header');
    assertEquals(2, groupHeaders.length, 'Should find two main group headers');
    const contentSettingsHeader = groupHeaders[1]!;
    const subGroupHeader = shadowRoot.querySelector<HTMLElement>(
        'div[role="heading"].setting-header');
    assertTrue(!!subGroupHeader, 'Sub-group header should exist.');

    assertFalse(
        contentSettingsHeader.hasAttribute('collapsed'),
        'Content Settings header should be expanded by default.');
    assertFalse(
        subGroupHeader.hasAttribute('collapsed'),
        'Sub-group header should be expanded by default.');

    assertTrue(
        !!subGroupHeader.offsetParent,
        'Sub-group should be rendered initially.');

    contentSettingsHeader.click();
    await microtasksFinished();

    assertTrue(
        contentSettingsHeader.hasAttribute('collapsed'),
        'Content Settings header should collapse after click.');
    assertEquals(
        null, subGroupHeader.offsetParent,
        'Sub-group should become hidden when parent collapses.');

    contentSettingsHeader.click();
    await microtasksFinished();

    assertFalse(
        contentSettingsHeader.hasAttribute('collapsed'),
        'Content Settings header should re-expand.');
    assertTrue(
        !!subGroupHeader.offsetParent, 'Sub-group should be visible again.');
    assertFalse(
        subGroupHeader.hasAttribute('collapsed'),
        'Sub-group header should have retained its expanded state.');

    subGroupHeader.click();
    await microtasksFinished();
    assertTrue(
        subGroupHeader.hasAttribute('collapsed'),
        'Sub-group header should collapse after its own click.');
  });
});

// Test suite for routing within the Privacy Sandbox Internals page.
suite('PrivacySandboxInternalsRoutingTest', function() {
  let page: InternalsPage;
  let shadowRoot: ShadowRoot;
  let tabContainer: HTMLElement;

  enum Page {
    TRACKING_PROTECTION = 'tracking-protection',
    ADVERTISING = 'advertising',
    CAPTURED_SURFACE_CONTROL = 'captured_surface_control',
    COOKIES = 'cookies',
    POPUPS = 'popups',
    TPCD_METADATA_GRANTS = 'tpcd_metadata_grants',
  }

  setup(async function() {
    const browserProxy = new TestPrivacySandboxInternalsBrowserProxy();
    browserProxy.setShouldShowTpcdMetadataGrants(true);
    PrivacySandboxInternalsBrowserProxy.setInstance(browserProxy);

    Router.resetInstanceForTesting();
    window.history.replaceState({}, '', window.location.pathname);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('internals-page');
    document.body.appendChild(page);
    shadowRoot = page.shadowRoot!;
    tabContainer = await waitForElement(shadowRoot, '#ps-page');
  });

  test('defaultsToFirstTabOnLoad', async function() {
    const allTabs =
        Array.from(shadowRoot.querySelectorAll<HTMLElement>('[slot="tab"]'));
    const firstSelectableTab =
        allTabs.find((tab) => tab.getAttribute('role') !== 'heading');
    const expectedIndex = allTabs.indexOf(firstSelectableTab!).toString();
    await waitForCondition(
        () => tabContainer.getAttribute('selected-index') === expectedIndex);
    const params = new URLSearchParams(window.location.search);
    assertEquals(Page.TRACKING_PROTECTION, params.get('page'));
  });

  test('switchesTabOnClickAndUpdateUrl', async () => {
    const cookiesTab = await waitForElement(
        shadowRoot, `div[slot="tab"][data-page-name="${Page.COOKIES}"]`);
    cookiesTab.click();

    const allTabs = Array.from(shadowRoot.querySelectorAll('[slot="tab"]'));
    const expectedIndex = allTabs.indexOf(cookiesTab).toString();

    await waitForCondition(
        () => tabContainer.getAttribute('selected-index') === expectedIndex);
    const params = new URLSearchParams(window.location.search);
    assertEquals(Page.COOKIES, params.get('page'));
  });

  test('navigatingToSpecificUrl', async () => {
    const targetPage = Page.COOKIES;
    Router.getInstance().navigateTo(targetPage);

    const cookiesTab = await waitForElement(
        shadowRoot, `div[slot="tab"][data-page-name="${targetPage}"]`);
    const allTabs = Array.from(shadowRoot.querySelectorAll('[slot="tab"]'));
    const expectedIndex = allTabs.indexOf(cookiesTab).toString();

    await waitForCondition(
        () => tabContainer.getAttribute('selected-index') === expectedIndex);
    const params = new URLSearchParams(window.location.search);
    assertEquals(
        targetPage, params.get('page'),
        'URL should be updated to the new page');
  });

  test('navigatesToDefaultOnInvalidUrl', async () => {
    window.history.replaceState({}, '', '?page=invalid-page');

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('internals-page');
    document.body.appendChild(page);
    shadowRoot = page.shadowRoot!;
    tabContainer = await waitForElement(shadowRoot, '#ps-page');

    const allTabs =
        Array.from(shadowRoot.querySelectorAll<HTMLElement>('[slot="tab"]'));
    const firstSelectableTab =
        allTabs.find((tab) => tab.getAttribute('role') !== 'heading');

    const expectedIndex = allTabs.indexOf(firstSelectableTab!).toString();

    await waitForCondition(
        () => tabContainer.getAttribute('selected-index') === expectedIndex);

    const params = new URLSearchParams(window.location.search);
    assertEquals(
        Page.TRACKING_PROTECTION, params.get('page'),
        'URL should be updated to the default page');
  });

  test('defaultsToFirstTabWhenNoPageInUrl', async function() {
    const allTabs =
        Array.from(shadowRoot.querySelectorAll<HTMLElement>('[slot="tab"]'));
    const firstSelectableTab =
        allTabs.find((tab) => tab.getAttribute('role') !== 'heading');

    const expectedIndex = allTabs.indexOf(firstSelectableTab!).toString();

    await waitForCondition(
        () => tabContainer.getAttribute('selected-index') === expectedIndex);
    const params = new URLSearchParams(window.location.search);
    assertEquals(
        Page.TRACKING_PROTECTION, params.get('page'),
        'URL should be updated to show the default page parameter.');
  });

  test('updatesTabWhenBackButtonIsUsed', async () => {
    Router.getInstance().navigateTo(Page.ADVERTISING);
    await waitForCondition(
        () => new URLSearchParams(window.location.search).get('page') ===
            Page.ADVERTISING);

    Router.getInstance().navigateTo(Page.POPUPS);
    await waitForCondition(
        () => new URLSearchParams(window.location.search).get('page') ===
            Page.POPUPS);

    history.back();

    await waitForCondition(
        () => new URLSearchParams(window.location.search).get('page') ===
            Page.ADVERTISING);

    const advertisingTab = await waitForElement(
        shadowRoot, `div[slot="tab"][data-page-name="${Page.ADVERTISING}"]`);
    const allTabs = Array.from(shadowRoot.querySelectorAll('[slot="tab"]'));
    const expectedIndex = allTabs.indexOf(advertisingTab).toString();
    assertEquals(expectedIndex, tabContainer.getAttribute('selected-index'));
  });

  test('updatesTabWhenForwardButtonIsUsed', async () => {
    Router.getInstance().navigateTo(Page.ADVERTISING);
    await waitForCondition(
        () => new URLSearchParams(window.location.search).get('page') ===
            Page.ADVERTISING);
    Router.getInstance().navigateTo(Page.POPUPS);
    await waitForCondition(
        () => new URLSearchParams(window.location.search).get('page') ===
            Page.POPUPS);

    history.back();
    await waitForCondition(
        () => new URLSearchParams(window.location.search).get('page') ===
            Page.ADVERTISING);

    history.forward();

    await waitForCondition(
        () => new URLSearchParams(window.location.search).get('page') ===
            Page.POPUPS);

    const popupsTab = await waitForElement(
        shadowRoot, `div[slot="tab"][data-page-name="${Page.POPUPS}"]`);
    const allTabs = Array.from(shadowRoot.querySelectorAll('[slot="tab"]'));
    const expectedIndex = allTabs.indexOf(popupsTab).toString();
    assertEquals(expectedIndex, tabContainer.getAttribute('selected-index'));
  });
});

// Test the <internals-page> element with the real PageHandler.
suite('InternalsPageTest', function() {
  let internalsPage: InternalsPage;
  let browserProxy: TestPrivacySandboxInternalsBrowserProxy;

  setup(async function() {
    browserProxy = new TestPrivacySandboxInternalsBrowserProxy();
    PrivacySandboxInternalsBrowserProxy.setInstance(browserProxy);

    const mockPrefs: PrivacySandboxInternalsPref[] = [
      {name: 'privacy_sandbox.some_pref', value: {boolValue: true}},
      {name: 'tracking_protection.some_pref', value: {boolValue: true}},
      {name: 'tpcd_experiment.some_pref', value: {boolValue: true}},
    ];

    browserProxy.testHandler.setPrefs(mockPrefs);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    internalsPage = document.createElement('internals-page');
    document.body.appendChild(internalsPage);
    await browserProxy.testHandler.whenCalled('readPrefsWithPrefixes');
  });

  test('rendersAdvertisingPrefs', async () => {
    Router.getInstance().navigateTo('advertising');
    const firstPrefElement = await waitForElement(
        internalsPage.shadowRoot!, '#advertising-prefs-panel > pref-display');
    assertTrue(
        !!firstPrefElement,
        'A <pref-display> element should be displayed for Advertising Prefs.');
  });

  test('rendersTrackingProtectionPrefs', async () => {
    const firstPrefGroupElement = await waitForElement(
        internalsPage.shadowRoot!,
        '#tracking-protection-prefs-panel > pref-display');
    assertTrue(
        !!firstPrefGroupElement,
        'A <pref-display> element should be displayed for Tracking Protection Prefs.');

    const secondPrefGroupElement = await waitForElement(
        internalsPage.shadowRoot!,
        '#tpcd-experiment-prefs-panel > pref-display');
    assertTrue(
        !!secondPrefGroupElement,
        'A <pref-display> element should be displayed for 3PCD Experiment Prefs.');
  });
});

// Tests the <internals-page> element's display of the TPCD tab based on feature
// flag status.
suite('PSInternalsPageTpcdTabLoadingTest', function() {
  let internalsPage: InternalsPage;
  let browserProxy: TestPrivacySandboxInternalsBrowserProxy;

  function setShouldShowTpcdMetadataGrants(isEnabled: boolean) {
    browserProxy = new TestPrivacySandboxInternalsBrowserProxy();
    browserProxy.setShouldShowTpcdMetadataGrants(isEnabled);
    PrivacySandboxInternalsBrowserProxy.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    internalsPage = document.createElement('internals-page');
    document.body.appendChild(internalsPage);
  }

  async function findTpcdTab() {
    const tabBox = await waitForElement(internalsPage.shadowRoot!, '#ps-page');
    if (!tabBox) {
      return false;
    }

    const tabs = tabBox.querySelectorAll<HTMLElement>('div[slot="tab"]');
    const foundTab = Array.from(tabs).find(
        (tab: HTMLElement) =>
            tab.textContent?.trim() === 'TPCD_METADATA_GRANTS');

    return foundTab;
  }

  test('hidesTpcdMetadataGrantsTab', async () => {
    setShouldShowTpcdMetadataGrants(false);
    const tpcdTab = await findTpcdTab();
    assertFalse(
        !!tpcdTab, 'The TPCD tab should not exist when its flag is disabled.');
  });

  test('rendersTpcdMetadataGrantsTab', async () => {
    setShouldShowTpcdMetadataGrants(true);
    const tpcdTab = await findTpcdTab();
    assertTrue(
        !!tpcdTab, 'The TPCD tab should exist when its flag is enabled.');
  });
});

// Test that the <mojo-timestamp> CustomElement renders the correct time.
suite('MojoTimestampElementTest', function() {
  let tsElement: HTMLElement;

  suiteSetup(async function() {
    await customElements.whenDefined('mojo-timestamp');
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    tsElement = document.createElement('mojo-timestamp');
    document.body.appendChild(tsElement);
  });

  const testTime = (ts: string, rendered: string) => {
    tsElement.setAttribute('ts', ts);
    const time = tsElement.shadowRoot!.querySelector('#time');
    assertTrue(!!time);
    assertEquals(time.textContent, rendered);
  };

  test('epoch', () => {
    testTime('0', 'epoch');
  });

  test('nearEpoch', () => {
    testTime('1', 'Mon, 01 Jan 1601 00:00:00 GMT');
    testTime('1000000', 'Mon, 01 Jan 1601 00:00:01 GMT');
  });

  test('aroundNow', () => {
    testTime('13348693565232806', 'Tue, 02 Jan 2024 18:26:05 GMT');
  });
});

// Test that the <mojo-timedelta> CustomElement renders the correct duration.
suite('MojoTimedeltaElementTest', function() {
  let element: HTMLElement;

  suiteSetup(async function() {
    await customElements.whenDefined('mojo-timedelta');
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('mojo-timedelta');
    document.body.appendChild(element);
  });

  const testDuration = (duration: string, rendered: string) => {
    element.setAttribute('duration', duration);
    const time = element.shadowRoot!.querySelector('#duration');
    assertTrue(!!time);
    assertEquals(time.textContent, rendered);
  };

  test('zero', () => {
    testDuration('0', '0 microseconds');
  });

  test('nonZero', () => {
    testDuration('213', '213 microseconds');
    testDuration('123456123456123456', '123456123456123456 microseconds');
  });
});

// Test the <value-display> and <expandable-json-viewer> elements.
suite('ValueDisplayElementTest', function() {
  let v: Value;
  let valueElement: ValueDisplayElement;
  const kPrefTitle = 'Some Pref Title';

  suiteSetup(async function() {
    await customElements.whenDefined('value-display');
    await customElements.whenDefined('expandable-json-viewer');
  });

  setup(function() {
    v = {} as Value;
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    valueElement = document.createElement('value-display');
    document.body.appendChild(valueElement);
  });

  const assertType = (s: string) => {
    const span = valueElement.$('#type');
    assertTrue(!!span);
    assertEquals(span.textContent, s);
  };

  const assertValue = (s: string) => {
    const span = valueElement.$('#value');
    assertTrue(!!span);
    assertEquals(span.textContent, s);
  };

  const assertJsonValue = (s: string) => {
    const jsonContainer = getExpandableJsonViewerElementOrFail();
    const jsonValueElement = jsonContainer.$('#json-value');
    assertTrue(!!jsonValueElement);
    assertEquals(jsonValueElement.textContent, s);
  };

  const getExpandableJsonViewerElementOrFail =
      (): ExpandableJsonViewerElement => {
        const span = valueElement.$('#value');
        assertTrue(!!span);
        const jsonContainer = span.querySelector('expandable-json-viewer');
        assertTrue(!!jsonContainer);
        return jsonContainer;
      };

  test('null', () => {
    v.nullValue = 1;
    valueElement.configure(v);
    const span = valueElement.$('#value');
    assertTrue(!!span);
    assertEquals(span.textContent, 'null');
    assertTrue(span.classList.contains('none'));
    assertType('');
  });

  test('trueBool', () => {
    v.boolValue = true;
    valueElement.configure(v);
    const span = valueElement.$('#value');
    assertTrue(!!span);
    assertEquals(span.textContent, 'true');
    assertTrue(span.classList.contains('bool-true'));
    assertType('');
  });

  test('falseBool', () => {
    v.boolValue = false;
    valueElement.configure(v);
    const span = valueElement.$('#value');
    assertTrue(!!span);
    assertEquals(span.textContent, 'false');
    assertTrue(span.classList.contains('bool-false'));
    assertType('');
  });

  test('int', () => {
    v.intValue = 867;
    valueElement.configure(v);
    assertValue('867');
    assertType('(int)');
  });

  test('string', () => {
    v.stringValue = 'all the small things';
    valueElement.configure(v);
    assertValue('all the small things');
    assertType('(string)');

    v.stringValue = '1234';
    valueElement.configure(v);
    assertValue('1234');
    assertType('(string)');
  });

  test('stringTimestamp', () => {
    v.stringValue = '12345';
    valueElement.configure(v, timestampLogicalFn);
    assertValue('12345');
    assertType('(string)');
    const span = valueElement.$('#logical-value');
    assertTrue(!!span);
    assertTrue(span.classList.contains('defined'));
    const mojoTs = span.querySelector('mojo-timestamp');
    assertTrue(!!mojoTs);
    assertEquals(mojoTs.getAttribute('ts'), '12345');
  });

  test('list', () => {
    v.listValue = {} as ListValue;
    v.listValue.storage = [1, 2, 3, 4].map((x) => {
      const v: Value = {} as Value;
      v.intValue = x;
      return v;
    });
    valueElement.configure(v, defaultLogicalFn, kPrefTitle);
    assertJsonValue(JSON.stringify(
        [{'intValue': 1}, {'intValue': 2}, {'intValue': 3}, {'intValue': 4}],
        null, 2));

    // Verify that <value-display> passes the title to expandable-json-viewer
    const jsonContainer = getExpandableJsonViewerElementOrFail();
    assertEquals(jsonContainer.getTitleTextForTesting(), kPrefTitle);
  });

  test('dictionary', async () => {
    v.dictionaryValue = {} as DictionaryValue;
    const v1: Value = {} as Value;
    v1.intValue = 10;
    const v2: Value = {} as Value;
    v2.stringValue = 'bikes';
    v.dictionaryValue.storage = {'v1': v1, 'v2': v2};
    valueElement.configure(v, defaultLogicalFn, kPrefTitle);
    await assertJsonValue(JSON.stringify(
        {'v1': {'intValue': 10}, 'v2': {'stringValue': 'bikes'}}, null, 2));

    const jsonContainer = getExpandableJsonViewerElementOrFail();
    assertEquals(jsonContainer.getTitleTextForTesting(), kPrefTitle);
  });

  test('flattens list with nested dictionary', () => {
    const vDict = {} as DictionaryValue;
    const v1: Value = {} as Value;
    v1.intValue = 10;
    const v2: Value = {} as Value;
    v2.stringValue = 'bikes';
    vDict.storage = {'v1': v1, 'v2': v2};

    v.listValue = {} as ListValue;
    v.listValue.storage = [
      {dictionaryValue: vDict} as Value,
    ];

    valueElement.configure(v);
    assertJsonValue(JSON.stringify(
        [{'v1': {'intValue': 10}, 'v2': {'stringValue': 'bikes'}}], null, 2));
  });

  test('flattens dictionary with nested list', () => {
    const vList = {} as ListValue;
    vList.storage = [1, 2, 3, 4].map((x) => {
      const v: Value = {} as Value;
      v.intValue = x;
      return v;
    });

    v.dictionaryValue = {} as DictionaryValue;
    v.dictionaryValue.storage = {
      'someKey': {listValue: vList} as Value,
    };

    valueElement.configure(v);
    assertJsonValue(JSON.stringify(
        {
          'someKey': [
            {'intValue': 1},
            {'intValue': 2},
            {'intValue': 3},
            {'intValue': 4},
          ],
        },
        null, 2));
  });

  test('binary', () => {
    v.binaryValue = [10, 20, 30, 40];
    valueElement.configure(v);
    assertValue('[10,20,30,40]');
    assertType('(binary)');
  });
});

// Test the <pref-display> element.
suite('PrefDisplayElementTest', function() {
  let v: Value;
  let prefDisplay: PrefDisplayElement;

  suiteSetup(async function() {
    await customElements.whenDefined('pref-display');
    await customElements.whenDefined('value-display');
  });

  setup(function() {
    v = {} as Value;
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    prefDisplay = document.createElement('pref-display');
    document.body.appendChild(prefDisplay);
  });

  const assertName = (s: string) => {
    const span = prefDisplay.$('.id-pref-name');
    assertTrue(!!span);
    assertEquals(span.textContent, s);
  };

  const getValueElementOrFail = () => {
    const span = prefDisplay.$('.id-pref-value');
    assertTrue(!!span);
    const value = span.querySelector('value-display');
    assertTrue(!!value);
    return value;
  };

  const assertType = (s: string) => {
    const vElem = getValueElementOrFail();
    const type = vElem.$('#type');
    assertTrue(!!type);
    assertEquals(type.textContent, s);
  };

  const assertValue = (s: string) => {
    const vElem = getValueElementOrFail();
    const value = vElem.$('#value');
    assertTrue(!!value);
    assertEquals(value.textContent, s);
  };

  const getLogicalValueElementOrFail = () => {
    const vElem = getValueElementOrFail();
    const value = vElem.$('#logical-value');
    assertTrue(!!value);
    return value;
  };

  const assertPrefLabelVisibilityIs = (visibility: boolean) => {
    const prefLabel = prefDisplay.$('.id-pref-label');
    assertTrue(!!prefLabel);
    assertEquals(prefLabel.hidden, !visibility);
  };

  test('basicStringPref', () => {
    v.stringValue = 'this is a string';
    prefDisplay.configure('foo', v);
    assertName('foo');

    assertType('(string)');
    assertValue('this is a string');
    assertEquals(getLogicalValueElementOrFail().children.length, 0);
    assertPrefLabelVisibilityIs(true);
  });

  test('basicIntPref', () => {
    v.intValue = 100;
    prefDisplay.configure('some.int', v);
    assertName('some.int');

    assertType('(int)');
    assertValue('100');
    assertEquals(getLogicalValueElementOrFail().children.length, 0);
    assertPrefLabelVisibilityIs(true);
  });

  test('logicalStringPref', () => {
    v.stringValue = '12345';
    prefDisplay.configure('some.timestamp', v, timestampLogicalFn);
    assertName('some.timestamp');

    assertType('(string)');
    assertValue('12345');
    const value = getLogicalValueElementOrFail();
    assertEquals(value.children.length, 1);
    const mojoTs = value.querySelector('mojo-timestamp');
    assertTrue(!!mojoTs);
    assertEquals(mojoTs.getAttribute('ts'), '12345');
    assertPrefLabelVisibilityIs(true);
  });

  test('hidesLabelForListValue', () => {
    v.listValue = {} as ListValue;
    v.listValue.storage = [1, 2, 3, 4].map((x) => {
      const v: Value = {} as Value;
      v.intValue = x;
      return v;
    });

    prefDisplay.configure('some.listvalue', v);
    assertPrefLabelVisibilityIs(false);
  });

  test('hidesLabelForDictionaryValue', () => {
    v.dictionaryValue = {} as DictionaryValue;
    const v1: Value = {} as Value;
    v1.intValue = 10;
    const v2: Value = {} as Value;
    v2.stringValue = 'bikes';
    v.dictionaryValue.storage = {'v1': v1, 'v2': v2};

    prefDisplay.configure('some.listvalue', v);
    assertPrefLabelVisibilityIs(false);
  });
});

// Test the <expandable-json-viewer> element.
suite('ExpandableJsonViewerElement', function() {
  let jsonViewer: ExpandableJsonViewerElement;
  const kJsonViewerTitle = 'JSON Viewer Title';
  const kJsonContent = '{}';

  suiteSetup(async function() {
    await customElements.whenDefined('text-copy-button');
    await customElements.whenDefined('expandable-json-viewer');
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    jsonViewer = document.createElement('expandable-json-viewer');
    document.body.appendChild(jsonViewer);
    const preElement = document.createElement('pre');
    preElement.innerText = kJsonContent;
    jsonViewer.configure(preElement, kJsonViewerTitle);
  });

  const getChildElementByIdOrFail = (id: string) => {
    const elem = jsonViewer.$(`#${id}`);
    assertTrue(!!elem);
    return elem;
  };

  test('rendersPassedChildElement', () => {
    const preElementFromDOM = jsonViewer.$('#json-content > pre');
    assertTrue(!!preElementFromDOM);
    assertEquals(preElementFromDOM.textContent, kJsonContent);
  });

  test('clickingJsonHeaderTogglesState', async () => {
    const jsonHeaderElement = jsonViewer.$('#json-header')!;

    assertEquals(jsonViewer.hasAttribute('expanded'), false);
    jsonHeaderElement.click();
    await microtasksFinished();
    assertEquals(jsonViewer.hasAttribute('expanded'), true);
    jsonHeaderElement.click();
    await microtasksFinished();
    assertEquals(jsonViewer.hasAttribute('expanded'), false);
  });

  test('rendersTitleInJsonHeader', () => {
    assertEquals(jsonViewer.getTitleTextForTesting(), kJsonViewerTitle);
  });

  test('clickingJsonHeaderSwitchesIcons', async () => {
    const jsonHeaderElement = jsonViewer.$('#json-header')!;
    const openIcon = getChildElementByIdOrFail('open-icon');
    const closeIcon = getChildElementByIdOrFail('close-icon');

    // Only shows open icon by default
    assertEquals(
        window.getComputedStyle(openIcon).getPropertyValue('display'), 'block');
    assertEquals(
        window.getComputedStyle(closeIcon).getPropertyValue('display'), 'none');

    // Check that only close-icon is visible when content is expanded
    jsonHeaderElement.click();
    await microtasksFinished();
    assertEquals(
        window.getComputedStyle(openIcon).getPropertyValue('display'), 'none');
    assertEquals(
        window.getComputedStyle(closeIcon).getPropertyValue('display'),
        'block');

    // Only open-icon is visible when collapsed after being expanded
    jsonHeaderElement.click();
    await microtasksFinished();
    assertEquals(
        window.getComputedStyle(openIcon).getPropertyValue('display'), 'block');
    assertEquals(
        window.getComputedStyle(closeIcon).getPropertyValue('display'), 'none');
  });

  test('clickingJsonHeaderTogglesJsonContentVisibility', async () => {
    const jsonHeaderElement = jsonViewer.$('#json-header')!;
    const jsonContent = getChildElementByIdOrFail('json-content');

    // Hides json-content by default
    assertEquals(
        window.getComputedStyle(jsonContent).getPropertyValue('height'), '0px');
    assertEquals(
        window.getComputedStyle(jsonContent).getPropertyValue('overflow'),
        'hidden');

    jsonHeaderElement.click();
    await microtasksFinished();
    assertNotEquals(
        window.getComputedStyle(jsonContent).getPropertyValue('height'), '0px');
    assertEquals(
        window.getComputedStyle(jsonContent).getPropertyValue('overflow'),
        'auto');

    jsonHeaderElement.click();
    await microtasksFinished();
    assertEquals(
        window.getComputedStyle(jsonContent).getPropertyValue('height'), '0px');
    assertEquals(
        window.getComputedStyle(jsonContent).getPropertyValue('overflow'),
        'hidden');
  });

  test('hasButtonToCopyContents', () => {
    const copyButton: TextCopyButton|null = jsonViewer.$('text-copy-button');
    assertTrue(!!copyButton);
    assertEquals(copyButton.getAttribute('text-to-copy'), kJsonContent);
  });

  test('clickingCopyButtonDoesNotChangeContainerExpandedState', async () => {
    const copyButton: TextCopyButton|null = jsonViewer.$('text-copy-button');
    assertTrue(!!copyButton);

    assertEquals(jsonViewer.hasAttribute('expanded'), false);
    copyButton.click();
    await microtasksFinished();
    assertEquals(jsonViewer.hasAttribute('expanded'), false);
  });
});

// Test the <text-copy-button> element.
suite('TextCopyButton', function() {
  let clipboardData = '';
  let textCopyButton: TextCopyButton;
  const kTextToCopy = 'Sample text';
  const textRecentlyCopiedAttribute = 'text-recently-copied';

  suiteSetup(async function() {
    await customElements.whenDefined('text-copy-button');

    const mockClipboard = {
      writeText: async (data: string) => {
        clipboardData = data;
        return Promise.resolve();
      },
      readText: async () => {
        return Promise.resolve(clipboardData);
      },
    };

    Object.defineProperty(navigator, 'clipboard', {
      configurable: true,
      get: () => mockClipboard,
    });
  });

  const getCopyIconElementOrFail = () => {
    const span = textCopyButton.$('.copy-icon');
    assertTrue(!!span);
    return span;
  };

  const getTickIconElementOrFail = () => {
    const span = textCopyButton.$('.tick-icon');
    assertTrue(!!span);
    return span;
  };

  suiteTeardown(function() {
    delete (navigator as any).clipboard;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    textCopyButton = document.createElement('text-copy-button');
    textCopyButton.setAttribute('text-to-copy', kTextToCopy);
    document.body.appendChild(textCopyButton);
  });

  test('clickingButtonCopiesTextFromAttribute', async () => {
    textCopyButton.click();
    const clipboardText = await navigator.clipboard.readText();
    assertEquals(clipboardText, kTextToCopy);
  });

  test('updatesTextToCopyWhenTextToCopyAttributeIsChanged', async () => {
    textCopyButton.click();
    let clipboardText = await navigator.clipboard.readText();
    assertEquals(clipboardText, kTextToCopy);

    textCopyButton.setAttribute('text-to-copy', 'updated text');
    textCopyButton.click();
    clipboardText = await navigator.clipboard.readText();
    assertEquals(clipboardText, 'updated text');
  });

  test('clickingButtonSetsRecentlyTextCopiedAttribute', async () => {
    assertFalse(textCopyButton.hasAttribute(textRecentlyCopiedAttribute));
    textCopyButton.click();
    await waitForCondition(
        () => textCopyButton.hasAttribute(textRecentlyCopiedAttribute));
    assertTrue(textCopyButton.hasAttribute(textRecentlyCopiedAttribute));
  });

  test('textCopiedAttributeGetsReverted', async () => {
    const mockTimer = new MockTimer();
    mockTimer.install();

    textCopyButton.click();
    // Awaiting navigator.clipboard.readText() allows us to make sure that the
    // writeText() call is completed. await waitForCondition() would have been
    // more ideal here, but MockTimer mocks setTimeout and prevents us from
    // being able to rely on waitForCondition.
    await navigator.clipboard.readText();
    await Promise.resolve();
    assertTrue(textCopyButton.hasAttribute(textRecentlyCopiedAttribute));
    mockTimer.tick(textCopyButton.revertIconWaitDuration);
    await Promise.resolve();
    assertFalse(textCopyButton.hasAttribute(textRecentlyCopiedAttribute));
    mockTimer.uninstall();
  });

  test('onClickIconVisibilityIsSetAndReverted', async () => {
    const mockTimer = new MockTimer();
    mockTimer.install();
    const copyIcon = getCopyIconElementOrFail();
    const tickIcon = getTickIconElementOrFail();

    // Just the copy icon should be shown by default
    assertEquals(
        window.getComputedStyle(copyIcon).getPropertyValue('display'), 'block');
    assertEquals(
        window.getComputedStyle(tickIcon).getPropertyValue('display'), 'none');

    // Just tick icon should be shown after the icon is clicked
    textCopyButton.click();
    await navigator.clipboard.readText();
    await Promise.resolve();
    assertEquals(
        window.getComputedStyle(copyIcon).getPropertyValue('display'), 'none');
    assertEquals(
        window.getComputedStyle(tickIcon).getPropertyValue('display'), 'block');

    // The check icon should be shown after the timeout
    mockTimer.tick(textCopyButton.revertIconWaitDuration);
    await Promise.resolve();
    assertEquals(
        window.getComputedStyle(copyIcon).getPropertyValue('display'), 'block');
    assertEquals(
        window.getComputedStyle(tickIcon).getPropertyValue('display'), 'none');


    mockTimer.uninstall();
  });
});


// Test the <pref-page> element
suite('PrefPageTest', function() {
  let prefPageParentElement: HTMLElement;

  const kPrefPageId = 'page-1';
  const kPrefPageTitle = 'Page 1';
  const kPrefGroup1Id = 'pref-group-1';
  const kPrefGroup2Id = 'pref-group-1';
  const kPrefGroup1Title = 'Group 1';
  const kPrefGroup2Title = 'Group 2';

  function createPrefGroup(id: string, title: string, prefPrefixes: string[]):
      PrivacySandboxInternalsPrefGroup {
    return {
      id,
      title,
      prefPrefixes,
    };
  }

  function createPrefPageConfig(
      id: string, title: string,
      prefGroups: PrivacySandboxInternalsPrefGroup[]):
      PrivacySandboxInternalsPrefPageConfig {
    return {
      id,
      title,
      prefGroups,
    };
  }

  const kPrefPageConfig = createPrefPageConfig(kPrefPageId, kPrefPageTitle, [
    createPrefGroup(kPrefGroup1Id, kPrefGroup1Title, []),
    createPrefGroup(kPrefGroup2Id, kPrefGroup2Title, []),
  ]);

  const kPrefPageConfigWithNoPrefGroups =
      createPrefPageConfig(kPrefPageId, kPrefPageTitle, []);

  suiteSetup(async function() {
    await customElements.whenDefined('pref-page');
  });

  async function setupPrefPageWithConfig(
      pageConfig: PrivacySandboxInternalsPrefPageConfig) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    prefPageParentElement = document.createElement('div');
    const prefPage = document.createElement('pref-page');
    prefPage.pageConfig = pageConfig;
    prefPageParentElement.appendChild(prefPage);
    document.body.appendChild(prefPageParentElement);

    // Wait for the <pref-page> to be removed from the DOM
    await waitForCondition(
        () => prefPageParentElement.querySelector('pref-page') === null);
  }

  test('createsTabSlotInParent', async () => {
    await setupPrefPageWithConfig(kPrefPageConfig);

    const page1TabSlot = await waitForElement(
        prefPageParentElement, `div[slot="tab"]#${kPrefPageId}-prefs-tab`);
    assertTrue(
        !!page1TabSlot,
        'A slot="tab" element should be created in the parent of pref-page');
    assertEquals(page1TabSlot.textContent, kPrefPageTitle);
    assertEquals(page1TabSlot.dataset['pageName'], kPrefPageId);
  });

  test('createsPanelSlotInParent', async () => {
    await setupPrefPageWithConfig(kPrefPageConfig);

    const page1PanelSlot =
        await waitForElement(prefPageParentElement, 'div[slot="panel"]');
    assertTrue(
        !!page1PanelSlot,
        'A slot="panel" element should be created in the parent of pref-page');
  });

  test('createsHeadingInParentForAllPrefGroups', async () => {
    await setupPrefPageWithConfig(kPrefPageConfig);

    const allHeadings = prefPageParentElement.querySelectorAll('h3');
    assertTrue(
        allHeadings.length === 2, 'A header should be created for each page');
    assertEquals(allHeadings[0]!.textContent, kPrefGroup1Title);
    assertEquals(allHeadings[1]!.textContent, kPrefGroup2Title);
  });

  test('createsPrefGroupPanelInParentForAllPrefGroups', async () => {
    await setupPrefPageWithConfig(kPrefPageConfig);

    const allPrefGroupPanels =
        prefPageParentElement.querySelectorAll('.pref-group-panel');
    assertTrue(
        allPrefGroupPanels.length === 2,
        'A pref-group-panel should be created for each pref group');
    assertEquals(allPrefGroupPanels[0]!.id, kPrefGroup1Id + '-prefs-panel');
    assertEquals(allPrefGroupPanels[1]!.id, kPrefGroup2Id + '-prefs-panel');
  });

  test('createsNoHeadingOrPrefGroupPanelWhenPrefGroupsIsEmpty', async () => {
    await setupPrefPageWithConfig(kPrefPageConfigWithNoPrefGroups);

    const allHeadings = prefPageParentElement.querySelectorAll('h3');
    assertTrue(
        allHeadings.length === 0,
        'No header should be created when prefGroups is empty');
    const allPrefGroupPanels =
        prefPageParentElement.querySelectorAll('.pref-group-panel');
    assertTrue(
        allPrefGroupPanels.length === 0,
        'No pref group panel should be created for each pref group');
  });
});
