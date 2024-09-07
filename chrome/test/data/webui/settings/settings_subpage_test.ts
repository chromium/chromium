// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsRoutes} from 'chrome://settings/settings.js';
import {loadTimeData, Route, Router} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {setupPopstateListener} from './test_util.js';

// clang-format on

suite('SettingsSubpage', function() {
  let testRoutes: {
    ABOUT: Route,
    ADVANCED: Route,
    BASIC: Route,
    CERTIFICATES: Route,
    COOKIE_DETAILS: Route,
    PEOPLE: Route,
    PRIVACY: Route,
    SEARCH_ENGINES: Route,
    SEARCH: Route,
    SITE_DATA: Route,
    SYNC: Route,
  };

  setup(function() {
    const basicRoute = new Route('/');
    const searchRoute = basicRoute.createSection('/search', 'search');
    const peopleRoute = basicRoute.createSection('/people', 'people');
    const privacyRoute = basicRoute.createSection('/privacy', 'privacy');
    const siteDataRoute = basicRoute.createChild('/siteData');

    testRoutes = {
      ABOUT: basicRoute.createChild('/about'),
      ADVANCED: basicRoute.createChild('/advanced'),
      BASIC: basicRoute,
      CERTIFICATES: privacyRoute.createChild('/certificates'),
      COOKIE_DETAILS: siteDataRoute.createChild('/cookies/detail'),
      PEOPLE: peopleRoute,
      PRIVACY: privacyRoute,
      SEARCH_ENGINES: searchRoute.createChild('/searchEngines'),
      SEARCH: searchRoute,
      SITE_DATA: siteDataRoute,
      SYNC: peopleRoute.createChild('/syncSetup'),
    };

    Router.resetInstanceForTesting(
        new Router(testRoutes as unknown as SettingsRoutes));

    setupPopstateListener();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  function createSettingsSubpageWithPreserveSearchTerm() {
    const subpage = document.createElement('settings-subpage');
    subpage.searchLabel = 'label';
    subpage.preserveSearchTerm = true;
    subpage.setAttribute('route-path', testRoutes.SITE_DATA.path);
    document.body.appendChild(subpage);
    return subpage;
  }

  test('help icon', function() {
    const subpage = document.createElement('settings-subpage');
    document.body.appendChild(subpage);
    flush();

    // Check that the help icon only shows up when a |learnMoreUrl| is
    // specified.
    assertFalse(
        !!subpage.shadowRoot!.querySelector('[cr-icon="cr:help-outline"]'));
    subpage.learnMoreUrl = 'https://www.chromium.org';
    flush();
    const icon = subpage.shadowRoot!.querySelector<HTMLElement>(
        '[cr-icon="cr:help-outline"]');
    assertTrue(!!icon);
    // Check that the icon is forced to always use 'ltr' mode.
    assertEquals('ltr', icon!.getAttribute('dir'));
    // Check that the icon has proper a11y label.
    subpage.pageTitle = 'Title';
    flush();
    assertEquals(
        icon.ariaLabel,
        subpage.i18n('subpageLearnMoreAriaLabel', subpage.pageTitle));
    assertEquals(
        icon?.getAttribute('aria-description'), subpage.i18n('opensInNewTab'));
  });

  test('favicon', function() {
    const subpage = document.createElement('settings-subpage');
    document.body.appendChild(subpage);
    flush();

    // No favicon is shown when the URL is not given.
    assertFalse(!!subpage.shadowRoot!.querySelector('site-favicon'));

    subpage.faviconSiteUrl = 'https://www.chromium.org';
    flush();

    // Favicon is shown when the URL is specified.
    const favicon = subpage.shadowRoot!.querySelector('site-favicon');
    assertTrue(!!favicon);
    assertEquals(subpage.faviconSiteUrl, favicon.url);
  });

  test('clear search (event)', function() {
    const subpage = document.createElement('settings-subpage');
    // Having a searchLabel will create the cr-search-field.
    subpage.searchLabel = 'test';
    document.body.appendChild(subpage);
    flush();
    const search = subpage.shadowRoot!.querySelector('cr-search-field');
    assertTrue(!!search);
    search!.setValue('Hello');
    subpage.dispatchEvent(new CustomEvent(
        'clear-subpage-search', {bubbles: true, composed: true}));
    flush();
    assertEquals('', search!.getValue());
  });

  test('clear search (click)', async () => {
    const subpage = document.createElement('settings-subpage');
    // Having a searchLabel will create the cr-search-field.
    subpage.searchLabel = 'test';
    document.body.appendChild(subpage);
    flush();
    const search = subpage.shadowRoot!.querySelector('cr-search-field');
    assertTrue(!!search);
    search!.setValue('Hello');
    assertEquals(null, search!.shadowRoot!.activeElement);
    search!.$.clearSearch.click();
    await flushTasks();
    assertEquals('', search!.getValue());
    assertEquals(search!.$.searchInput, search!.shadowRoot!.activeElement);
  });

  test('preserve search result when back button is clicked', async () => {
    // Load settings subpage.
    Router.getInstance().navigateTo(testRoutes.SITE_DATA);
    let subpage = createSettingsSubpageWithPreserveSearchTerm();
    flush();

    // Set search field.
    let search = subpage.shadowRoot!.querySelector('cr-search-field');
    assertTrue(!!search);
    search!.setValue('test');
    assertEquals('test', search!.getValue());

    // Navigate to another subpage.
    Router.getInstance().navigateTo(testRoutes.COOKIE_DETAILS);
    subpage.remove();

    // Go back to settings subpage, verify search field is restored.
    Router.getInstance().navigateToPreviousRoute();
    subpage = createSettingsSubpageWithPreserveSearchTerm();
    await eventToPromise('popstate', window);
    search = subpage.shadowRoot!.querySelector('cr-search-field');
    assertTrue(!!search);
    assertEquals('test', search!.getValue());

    // Go back to settings subpage, verify search field is empty
    Router.getInstance().navigateToPreviousRoute();
    subpage = createSettingsSubpageWithPreserveSearchTerm();
    await eventToPromise('popstate', window);
    search = subpage.shadowRoot!.querySelector('cr-search-field');
    assertTrue(!!search);
    assertEquals('', search!.getValue());
  });

  test('preserve search result from URL input', async function() {
    const params = new URLSearchParams();
    params.append('searchSubpage', 'test');
    Router.getInstance().navigateTo(testRoutes.SITE_DATA, params);
    const subpage = createSettingsSubpageWithPreserveSearchTerm();
    await flushTasks();
    const search = subpage.shadowRoot!.querySelector('cr-search-field');
    assertTrue(!!search);
    assertEquals('test', search!.getValue());
  });

  test('navigates to parent when there is no history', function() {
    // Pretend that we initially started on the CERTIFICATES route.
    window.history.replaceState(undefined, '', testRoutes.CERTIFICATES.path);
    Router.getInstance().initializeRouteFromUrl();
    assertEquals(
        testRoutes.CERTIFICATES, Router.getInstance().getCurrentRoute());

    const subpage = document.createElement('settings-subpage');
    document.body.appendChild(subpage);

    subpage.shadowRoot!.querySelector('cr-icon-button')!.click();
    assertEquals(testRoutes.PRIVACY, Router.getInstance().getCurrentRoute());
  });

  test('navigates to any route via window.back()', async () => {
    Router.getInstance().navigateTo(testRoutes.BASIC);
    Router.getInstance().navigateTo(testRoutes.SYNC);
    assertEquals(testRoutes.SYNC, Router.getInstance().getCurrentRoute());

    const subpage = document.createElement('settings-subpage');
    document.body.appendChild(subpage);

    subpage.shadowRoot!.querySelector('cr-icon-button')!.click();

    await eventToPromise('popstate', window);
    assertEquals(testRoutes.BASIC, Router.getInstance().getCurrentRoute());
  });

  test('updates the title of the document when active', function() {
    const expectedTitle = 'My Subpage Title';
    Router.getInstance().navigateTo(testRoutes.SEARCH);
    const subpage = document.createElement('settings-subpage');
    subpage.setAttribute('route-path', testRoutes.SEARCH_ENGINES.path);
    subpage.setAttribute('page-title', expectedTitle);
    document.body.appendChild(subpage);

    Router.getInstance().navigateTo(testRoutes.SEARCH_ENGINES);
    assertEquals(
        document.title,
        loadTimeData.getStringF('settingsAltPageTitle', expectedTitle));
  });
});

suite('SettingsSubpageSearch', function() {
  test('host autofocus propagates to <cr-input>', function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const element = document.createElement('cr-search-field');
    element.toggleAttribute('autofocus', true);
    document.body.appendChild(element);

    assertTrue(element.shadowRoot!.querySelector('cr-input')!.hasAttribute(
        'autofocus'));

    element.removeAttribute('autofocus');
    assertFalse(element.shadowRoot!.querySelector('cr-input')!.hasAttribute(
        'autofocus'));
  });
});
