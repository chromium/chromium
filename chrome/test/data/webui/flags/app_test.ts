// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://flags/app.js';

import type {FlagsAppElement} from 'chrome://flags/app.js';
import type {ExperimentalFeaturesData, Feature} from 'chrome://flags/flags_browser_proxy.js';
import {FlagsBrowserProxyImpl} from 'chrome://flags/flags_browser_proxy.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestFlagsBrowserProxy} from './test_flags_browser_proxy.js';

suite('FlagsAppTest', function() {
  const supportedFeatures: Feature[] = [
    {
      // Experiment with default option
      'description': 'available feature',
      'internal_name': 'available-feature',
      'is_default': true,
      'name': 'available feature',
      'enabled': true,
      'options': [
        {
          'description': 'Default',
          'internal_name': 'available-feature@0',
          'selected': false,
        },
        {
          'description': 'Enabled',
          'internal_name': 'available-feature@1',
          'selected': false,
        },
        {
          'description': 'Disabled',
          'internal_name': 'available-feature@2',
          'selected': false,
        },
      ],
      'supported_platforms': ['Windows'],
    },
    {
      // Experiment without default option
      'description': 'availabl feature non default',
      'internal_name': 'available-feature-non-default',
      'is_default': true,
      'name': 'available feature non default',
      'enabled': false,
      'supported_platforms': ['Windows'],
    },
  ];
  const unsupportedFeatures: Feature[] = [
    {
      'description': 'unavailable feature',
      'enabled': false,
      'internal_name': 'unavailable-feature',
      'is_default': true,
      'name': 'unavailable feature',
      'supported_platforms': ['ChromeOS', 'Android'],
    },
  ];
  const experimentalFeaturesData: ExperimentalFeaturesData = {
    'supportedFeatures': supportedFeatures,
    'unsupportedFeatures': unsupportedFeatures,
    'needsRestart': false,
    'showBetaChannelPromotion': false,
    'showDevChannelPromotion': false,
    // <if expr="chromeos_ash">
    'showOwnerWarning': true,
    'showSystemFlagsLink': false,
    // </if>
    // <if expr="chromeos_lacros">
    'showSystemFlagsLink': true,
    // </if>
  };

  let app: FlagsAppElement;
  let searchTextArea: HTMLInputElement;
  let clearSearch: HTMLInputElement;
  let resetAllButton: HTMLButtonElement;
  let browserProxy: TestFlagsBrowserProxy;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestFlagsBrowserProxy();
    browserProxy.setFeatureData(experimentalFeaturesData);
    FlagsBrowserProxyImpl.setInstance(browserProxy);
    app = document.createElement('flags-app');
    document.body.appendChild(app);
    app.setAnnounceStatusDelayMsForTesting(0);
    app.setSearchDebounceDelayMsForTesting(0);
    await app.experimentalFeaturesReadyForTesting();
    searchTextArea = app.getRequiredElement<HTMLInputElement>('#search');
    clearSearch = app.getRequiredElement<HTMLInputElement>('.clear-search');
    resetAllButton =
        app.getRequiredElement<HTMLButtonElement>('#experiment-reset-all');
  });

  function searchBoxInput(text: string) {
    searchTextArea.value = text;
    searchTextArea.dispatchEvent(
        new CustomEvent('input', {composed: true, bubbles: true}));
  }

  test('Layout', function() {
    // Flag search
    assertTrue(isVisible(searchTextArea));
    assertFalse(isVisible(clearSearch));
    assertTrue(isVisible(resetAllButton));

    // <if expr="chromeos_ash">
    assertFalse(isVisible(app.getRequiredElement('#os-link-container')));
    // </if>
    // <if expr="chromeos_lacros">
    assertTrue(isVisible(app.getRequiredElement('#os-link-container')));
    // </if>

    // Title and version
    assertTrue(isVisible(app.getRequiredElement('.section-header-title')));
    assertTrue(isVisible(app.getRequiredElement('#version')));

    // Blurb warning
    assertTrue(isVisible(app.getRequiredElement('.blurb-container')));
    // <if expr="chromeos_ash">
    // Owner warning
    assertTrue(!!app.getRequiredElement('#owner-warning'));
    // </if>
  });

  test(
      'check available/unavailable tabs are rendered properly',
      async function() {
        const crTabs = app.getRequiredElement('cr-tabs');
        assertTrue(isVisible(crTabs));
        assertEquals(2, crTabs.tabNames.length);
        assertEquals(0, crTabs.selected);

        const defaultAvailableExperimentsContainer =
            app.getRequiredElement('#default-experiments');
        const nonDefaultAvailableExperimentsContainer =
            app.getRequiredElement('#non-default-experiments');
        const unavailableExperimentsContainer =
            app.getRequiredElement('#unavailable-experiments');
        assertFalse(isVisible(nonDefaultAvailableExperimentsContainer));
        assertTrue(isVisible(defaultAvailableExperimentsContainer));
        assertFalse(isVisible(unavailableExperimentsContainer));

        // Toggle unavailable tab and the unavailable experiments container
        // becomes visible.
        const tabs = crTabs.shadowRoot!.querySelectorAll<HTMLElement>('.tab');
        assertEquals(2, tabs.length);
        tabs[1]!.click();
        await microtasksFinished();
        assertEquals(1, crTabs.selected);
        assertFalse(isVisible(nonDefaultAvailableExperimentsContainer));
        assertFalse(isVisible(defaultAvailableExperimentsContainer));
        assertTrue(isVisible(unavailableExperimentsContainer));
      });

  test('clear search button shown/hidden', async function() {
    // The clear search button is hidden initially.
    assertFalse(isVisible(clearSearch));

    // The clear search button is shown when an input event fired.
    const searchEventPromise =
        eventToPromise('search-finished-for-testing', app);
    searchBoxInput('test');
    await searchEventPromise;
    assertTrue(isVisible(clearSearch));

    // The clear search button is hidden after clicked.
    clearSearch.click();
    await microtasksFinished();
    assertEquals('', searchTextArea.value);
    assertFalse(isVisible(clearSearch));
  });

  test('ResetAllClick', async function() {
    const restartToast = app.getRequiredElement('#needs-restart');
    const restartButton =
        app.getRequiredElement<HTMLButtonElement>('#experiment-restart-button');

    // The restart toast is not visible initially.
    assertFalse(restartToast.classList.contains('show'));
    // The restartButton should be disabled so that it is not in the tab order.
    assertTrue(restartButton.disabled);

    let defaultExperiments = app.getRequiredElement('#default-experiments')
                                 .querySelectorAll('flags-experiment');
    assertEquals(supportedFeatures.length, defaultExperiments.length);

    // Need to turn `needsRestart` to true, so that the toast is not dismissed
    // after re-fetching the backend data below.
    const data: ExperimentalFeaturesData =
        structuredClone(experimentalFeaturesData);
    data.needsRestart = true;
    browserProxy.setFeatureData(data);

    // The "Reset all" button is clicked, the restart toast becomes visible and
    // the experiment data is re-requested from the backend.
    browserProxy.reset();
    resetAllButton.click();
    assertTrue(restartToast.classList.contains('show'));
    await browserProxy.whenCalled('requestExperimentalFeatures');

    // Check that the same number of experiments is rendered after re-rendering.
    defaultExperiments = app.getRequiredElement('#default-experiments')
                             .querySelectorAll('flags-experiment');
    assertEquals(supportedFeatures.length, defaultExperiments.length);

    // The restart button is clicked and a request to restart is sent.
    assertFalse(restartButton.disabled);
    restartButton.click();
    return browserProxy.whenCalled('restartBrowser');
  });

  test('search and found match', function() {
    const promise = eventToPromise('search-finished-for-testing', app);
    searchBoxInput('available');
    return promise.then(() => {
      assertFalse(isVisible(app.getRequiredElement('.no-match')));
      const noMatchMsg = app.shadowRoot!.querySelectorAll<HTMLElement>(
          '.tab-content .no-match');
      assertTrue(!!noMatchMsg[0]);
      assertEquals(
          2,
          app.shadowRoot!
              .querySelectorAll(
                  `#tab-content-available flags-experiment:not([hidden])`)
              .length);
      assertTrue(!!noMatchMsg[1]);
      assertEquals(
          1,
          app.shadowRoot!
              .querySelectorAll(
                  `#tab-content-unavailable flags-experiment:not([hidden])`)
              .length);
    });
  });

  test('search and match not found', function() {
    const promise = eventToPromise('search-finished-for-testing', app);
    searchBoxInput('none');
    return promise.then(() => {
      assertTrue(isVisible(app.getRequiredElement('.no-match')));
      const noMatchMsg = app.shadowRoot!.querySelectorAll<HTMLElement>(
          '.tab-content .no-match');
      assertTrue(!!noMatchMsg[0]);
      assertEquals(
          0,
          app.shadowRoot!
              .querySelectorAll(
                  `#tab-content-available flags-experiment:not([hidden])`)
              .length);
      assertTrue(!!noMatchMsg[1]);
      assertEquals(
          0,
          app.shadowRoot!
              .querySelectorAll(
                  `#tab-content-unavailable flags-experiment:not([hidden])`)
              .length);
    });
  });

  test('SearchFieldFocusTest', function() {
    // Search field is focused on page load.
    assertEquals(searchTextArea, getDeepActiveElement());

    // Remove focus on search field.
    searchTextArea.blur();

    // Clear search.
    searchBoxInput('test');
    clearSearch.click();

    // Search field is focused after search is cleared.
    assertEquals(searchTextArea, getDeepActiveElement());

    // Dispatch 'Enter' keyboard event and check that search remains focused.
    searchBoxInput('test');
    window.dispatchEvent(new KeyboardEvent('keyup', {key: 'Enter'}));
    assertEquals(searchTextArea, getDeepActiveElement());

    // Dispatch 'Escape' keyboard event and check that search is cleard and not
    // focused.
    window.dispatchEvent(new KeyboardEvent('keyup', {key: 'Escape'}));
    assertNotEquals(searchTextArea, getDeepActiveElement());
  });
});
