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
    // <if expr="is_chromeos">
    'showOwnerWarning': true,
    // </if>
  };

  let app: FlagsAppElement;
  let searchTextArea: HTMLInputElement;
  let clearSearch: HTMLInputElement;
  let resetAllButton: HTMLButtonElement;
  let browserProxy: TestFlagsBrowserProxy;

  async function setupApp(data: ExperimentalFeaturesData) {
    browserProxy.setFeatureData(data);
    app = document.createElement('flags-app');
    document.body.appendChild(app);
    app.setAnnounceStatusDelayMsForTesting(0);
    app.setSearchDebounceDelayMsForTesting(0);
    await app.experimentalFeaturesReadyForTesting();
    searchTextArea = app.getRequiredElement<HTMLInputElement>('#search');
    clearSearch = app.getRequiredElement<HTMLInputElement>('.clear-search');
    resetAllButton =
        app.getRequiredElement<HTMLButtonElement>('#experiment-reset-all');
  }

  function assertRestartNeeded(state: boolean) {
    const restartToast = app.getRequiredElement('#needs-restart');
    assertEquals(state ? 'alert' : 'none', restartToast.role);
    assertEquals(state, restartToast.hasAttribute('show'));

    const restartButton =
        app.getRequiredElement<HTMLButtonElement>('#experiment-restart-button');
    assertEquals(state, !restartButton.disabled);
  }

  function simulateSelectChange(select: HTMLSelectElement, index: number) {
    select.selectedIndex = index;
    select.dispatchEvent(new Event('change'));
    return microtasksFinished();
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestFlagsBrowserProxy();
    FlagsBrowserProxyImpl.setInstance(browserProxy);
    return setupApp(experimentalFeaturesData);
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

    // Title and version
    assertTrue(isVisible(app.getRequiredElement('.section-header-title')));
    assertTrue(isVisible(app.getRequiredElement('#version')));

    // Blurb warning
    assertTrue(isVisible(app.getRequiredElement('.blurb-container')));
    // <if expr="is_chromeos">
    // Owner warning
    assertTrue(!!app.getRequiredElement('#owner-warning'));
    // </if>
  });

  test('AvailableUnavailableTabsRendered', async function() {
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
    const tabs = crTabs.shadowRoot.querySelectorAll<HTMLElement>('.tab');
    assertEquals(2, tabs.length);
    tabs[1]!.click();
    await microtasksFinished();
    assertEquals(1, crTabs.selected);
    assertFalse(isVisible(nonDefaultAvailableExperimentsContainer));
    assertFalse(isVisible(defaultAvailableExperimentsContainer));
    assertTrue(isVisible(unavailableExperimentsContainer));
  });

  test('DefaultNonDefaultExperimentsRendered', async function() {
    function getDefaultEntries() {
      return app.shadowRoot.querySelectorAll(
          '#default-experiments flags-experiment');
    }

    function getNonDefaultEntries() {
      return app.shadowRoot.querySelectorAll(
          '#non-default-experiments flags-experiment');
    }

    // Check that the dummy data has two experiments in their default state.
    assertEquals(2, experimentalFeaturesData.supportedFeatures.length);
    assertTrue(experimentalFeaturesData.supportedFeatures[0]!.is_default);
    assertTrue(experimentalFeaturesData.supportedFeatures[1]!.is_default);

    // Check that they are rendered under the corresponding sections on startup.
    assertEquals(2, getDefaultEntries().length);
    assertEquals(0, getNonDefaultEntries().length);

    // Simulate case where one experiment is in default state, and one isn't.
    const data: ExperimentalFeaturesData =
        structuredClone(experimentalFeaturesData);
    data.supportedFeatures[1]!.is_default = false;
    await setupApp(data);

    // Check that they are rendered under the corresponding sections on startup.
    assertEquals(1, getDefaultEntries().length);
    assertEquals(1, getNonDefaultEntries().length);
  });

  test('RestartButtonTabOrder', async function() {
    const experiments = app.getRequiredElement('#default-experiments')
                            .querySelectorAll('flags-experiment');
    assertEquals(2, experiments.length);

    // Focus the first experiment's <select> and simulate a change.
    const select = experiments[0]!.shadowRoot.querySelector('select');
    assertTrue(!!select);
    select.focus();
    assertEquals(select, getDeepActiveElement());
    await simulateSelectChange(select, 1);

    // Simulate a 'Tab' keystroke, and check that the 'restart' button is
    // focused, instead of the next experiment.
    select.dispatchEvent(new KeyboardEvent('keydown', {key: 'Tab'}));
    const restartButton =
        app.getRequiredElement<HTMLButtonElement>('#experiment-restart-button');
    assertEquals(restartButton, getDeepActiveElement());

    // Simulate 'Shift+Tab' keystroke on the 'restart' button. The previously
    // changed <select> should be focused.
    restartButton.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Tab', shiftKey: true}));
    assertEquals(select, getDeepActiveElement());

    // Simulate another 'Tab' keystroke. This time the next experiment should be
    // focused and not the 'restart' button.
    select.dispatchEvent(new KeyboardEvent('keydown', {key: 'Tab'}));
    // Normally the next experiment's permalink should be focused, but
    // simulating a 'Tab' event from JS does not trigger the same actions as a
    // real user-gesture. For the purposes of this test checking that the focus
    // does not go back to the 'restart' button is sufficient (guarantees that
    // the select's 'blur' handler did its job).
    assertNotEquals(restartButton, getDeepActiveElement());
  });

  test('ClearSearchButtonVisibility', async function() {
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
    assertRestartNeeded(false);

    let defaultExperiments = app.getRequiredElement('#default-experiments')
                                 .querySelectorAll('flags-experiment');
    assertEquals(supportedFeatures.length, defaultExperiments.length);

    // The "Reset all" button is clicked, the restart toast becomes visible and
    // the experiment data is re-requested from the backend.
    browserProxy.reset();
    resetAllButton.click();
    await microtasksFinished();

    assertRestartNeeded(true);
    await browserProxy.whenCalled('requestExperimentalFeatures');

    // Check that the same number of experiments is rendered after re-rendering.
    defaultExperiments = app.getRequiredElement('#default-experiments')
                             .querySelectorAll('flags-experiment');
    assertEquals(supportedFeatures.length, defaultExperiments.length);

    // The restart button is clicked and a request to restart is sent.
    assertRestartNeeded(true);
    const restartButton =
        app.getRequiredElement<HTMLButtonElement>('#experiment-restart-button');
    restartButton.click();
    return browserProxy.whenCalled('restartBrowser');
  });

  test('SearchMatchFound', async function() {
    const promise = eventToPromise('search-finished-for-testing', app);
    searchBoxInput('available');
    await promise;

    assertFalse(isVisible(app.getRequiredElement('.no-match')));
    const noMatchMsg =
        app.shadowRoot.querySelectorAll<HTMLElement>('.tab-content .no-match');
    assertTrue(!!noMatchMsg[0]);
    assertEquals(
        2,
        app.shadowRoot
            .querySelectorAll(
                `#tab-content-available flags-experiment:not([hidden])`)
            .length);
    assertTrue(!!noMatchMsg[1]);
    assertEquals(
        1,
        app.shadowRoot
            .querySelectorAll(
                `#tab-content-unavailable flags-experiment:not([hidden])`)
            .length);
  });

  test('SearchMatchNotFound', async function() {
    const promise = eventToPromise('search-finished-for-testing', app);
    searchBoxInput('none');
    await promise;

    assertTrue(isVisible(app.getRequiredElement('.no-match')));
    const noMatchMsg =
        app.shadowRoot.querySelectorAll<HTMLElement>('.tab-content .no-match');
    assertTrue(!!noMatchMsg[0]);
    assertEquals(
        0,
        app.shadowRoot
            .querySelectorAll(
                '#tab-content-available flags-experiment:not([hidden])')
            .length);
    assertTrue(!!noMatchMsg[1]);
    assertEquals(
        0,
        app.shadowRoot
            .querySelectorAll(
                '#tab-content-unavailable flags-experiment:not([hidden])')
            .length);
  });

  test('SearchFieldFocusTest', function() {
    // Search field is focused on page load.
    assertEquals(searchTextArea, getDeepActiveElement());

    // Dispatch 'Escape' keyboard event and check that search is blurred.
    window.dispatchEvent(new KeyboardEvent('keyup', {key: 'Escape'}));
    assertEquals(document.body, getDeepActiveElement());

    // Dispatch '/' keyboard event and check that search is focused.
    window.dispatchEvent(new KeyboardEvent('keyup', {key: '/'}));
    assertEquals(searchTextArea, getDeepActiveElement());

    // Remove focus from search field.
    searchTextArea.blur();
    assertEquals(document.body, getDeepActiveElement());

    // Clear search.
    searchBoxInput('test');
    clearSearch.click();

    // Search field is focused after search is cleared.
    assertEquals(searchTextArea, getDeepActiveElement());
  });


  // Test the case where the user searches, then changes a feature's value, then
  // clears search.
  test('Search_Change_ClearSearch', async function() {
    // Assert initial state.
    assertRestartNeeded(false);

    // Simulate search and assert results were found.
    const promise = eventToPromise('search-finished-for-testing', app);
    searchBoxInput('available');
    await promise;
    const experiments = app.shadowRoot.querySelectorAll(
        '#tab-content-available flags-experiment:not([hidden])');
    assertEquals(2, experiments.length);

    // Simulate changing the experiment's state.
    const select = experiments[0]!.shadowRoot!.querySelector('select');
    assertTrue(!!select);
    await simulateSelectChange(select, 1);
    assertRestartNeeded(true);

    // Simulate changing again to the original value.
    await simulateSelectChange(select, 0);
    assertRestartNeeded(true);

    // Simulate clearing search.
    clearSearch.click();
    await microtasksFinished();
    assertRestartNeeded(true);
  });
});
