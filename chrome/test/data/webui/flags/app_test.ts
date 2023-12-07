// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://flags/app.js';

import {FlagsAppElement} from 'chrome://flags/app.js';
import {ExperimentalFeaturesData, Feature, FlagsBrowserProxyImpl} from 'chrome://flags/flags_browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

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
    'showOwnerWarning': false,
    'showSystemFlagsLink': true,
  };

  let app: FlagsAppElement;
  let searchTextArea: HTMLInputElement;
  let clearSearch: HTMLInputElement;
  let resetAllButton: HTMLButtonElement;
  let browserProxy: TestFlagsBrowserProxy;

  setup(async function() {
    browserProxy = new TestFlagsBrowserProxy();
    browserProxy.setFeatureData(experimentalFeaturesData);
    FlagsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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

  function selectChange(selectEl: HTMLSelectElement, index: number) {
    selectEl.selectedIndex = index;
    selectEl.dispatchEvent(
        new CustomEvent('change', {composed: true, bubbles: true}));
  }

  test('check available/unavailable tabs are rendered properly', function() {
    const availableTab = app.getRequiredElement('#tab-available');
    const unavailableTab = app.getRequiredElement('#tab-unavailable');

    assertTrue(isVisible(availableTab));
    assertTrue(isVisible(unavailableTab));

    const defaultAvailableExperimentsContainer =
        app.getRequiredElement('#default-experiments');
    assertTrue(isVisible(defaultAvailableExperimentsContainer));

    const nonDefaultAvailableExperimentsContainer =
        app.getRequiredElement('#non-default-experiments');
    assertFalse(isVisible(nonDefaultAvailableExperimentsContainer));

    const unavailableExperimentsContainer =
        app.getRequiredElement('#unavailable-experiments');
    assertFalse(isVisible(unavailableExperimentsContainer));

    // Toggle unavailable tab and the unavailable experiments container becomes
    // visible.
    unavailableTab.click();
    assertTrue(isVisible(unavailableExperimentsContainer));
    assertFalse(isVisible(defaultAvailableExperimentsContainer));
  });

  test(
      'enable experiment and selectExperimentalFeature event fired',
      function() {
        const experimentWithDefault =
            app.getRequiredElement('#default-experiments')
                .querySelectorAll('flags-experiment')[0];
        assertTrue(!!experimentWithDefault);
        const select =
            experimentWithDefault.getRequiredElement<HTMLSelectElement>(
                '.experiment-select');
        assertTrue(!!select);

        // Initially, the selected option is "Default" at index 0
        assertEquals(0, select.selectedIndex);

        // Select the "Enabled" option at index 1
        selectChange(select, 1);
        return browserProxy.whenCalled('selectExperimentalFeature');
      });

  test(
      'enable experiment and enableExperimentalFeature event fired',
      function() {
        const experimentWithNoDefault =
            app.getRequiredElement('#default-experiments')
                .querySelectorAll('flags-experiment')[1];
        assertTrue(!!experimentWithNoDefault);
        const select =
            experimentWithNoDefault.getRequiredElement<HTMLSelectElement>(
                '.experiment-enable-disable');
        assertTrue(!!select);

        // Select the non-default option at index 1
        selectChange(select, 1);
        return browserProxy.whenCalled('enableExperimentalFeature');
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

    // The clear search button is pressed then search text is cleared and button
    // is hidden
    clearSearch.click();
    assertEquals('', searchTextArea.value);
    assertFalse(isVisible(clearSearch));
  });

  test('restart toast shown and relaunch event fired', function() {
    const restartToast = app.getRequiredElement('#needs-restart');

    // The restart toast is not visible initially.
    assertFalse(restartToast.classList.contains('show'));

    // The reset all button is clicked and restart toast becomes visible.
    resetAllButton.click();
    assertTrue(restartToast.classList.contains('show'));

    // The restart button is clicked and a browserRestart event fired.
    const restartButton =
        app.getRequiredElement<HTMLButtonElement>('#experiment-restart-button');
    restartButton.click();
    return browserProxy.whenCalled('restartBrowser');
  });

  test('search and found match', function() {
    const promise = eventToPromise('search-finished-for-testing', app);
    searchBoxInput('available');
    return promise.then(() => {
      assertFalse(isVisible(app.getRequiredElement('.no-match')));
      const noMatchMsg: NodeListOf<HTMLElement> =
          app.$all('.tab-content .no-match');
      assertTrue(!!noMatchMsg[0]);
      assertEquals(
          2,
          app.$all(`#tab-content-available flags-experiment:not(.hidden)`)
              .length);
      assertTrue(!!noMatchMsg[1]);
      assertEquals(
          1,
          app.$all(`#tab-content-unavailable flags-experiment:not(.hidden)`)
              .length);
    });
  });

  test('search and match not found', function() {
    const promise = eventToPromise('search-finished-for-testing', app);
    searchBoxInput('none');
    return promise.then(() => {
      assertTrue(isVisible(app.getRequiredElement('.no-match')));
      const noMatchMsg: NodeListOf<HTMLElement> =
          app.$all('.tab-content .no-match');
      assertTrue(!!noMatchMsg[0]);
      assertEquals(
          0,
          app.$all(`#tab-content-available flags-experiment:not(.hidden)`)
              .length);
      assertTrue(!!noMatchMsg[1]);
      assertEquals(
          0,
          app.$all(`#tab-content-unavailable flags-experiment:not(.hidden)`)
              .length);
    });
  });
});
