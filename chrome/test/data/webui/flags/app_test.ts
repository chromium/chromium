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
      'description': 'available_feature_1',
      'internal_name': 'available_feature_1',
      'is_default': true,
      'name': 'available_feature_1',
      'enabled': true,
      'options': [
        {
          'description': 'Default',
          'internal_name': 'available_feature_1',
          'selected': false,
        },
        {
          'description': 'Enabled',
          'internal_name': 'available_feature_1',
          'selected': false,
        },
        {
          'description': 'Disabled',
          'internal_name': 'available_feature_1',
          'selected': false,
        },
      ],
      'supported_platforms': ['Windows'],
    },
  ];
  const unsupportedFeatures: Feature[] = [
    {
      'description': 'unavailable_feature_1',
      'enabled': false,
      'internal_name': 'unavailable_feature_1',
      'is_default': true,
      'name': 'unavailable_feature_1',
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

  test('clear search button shown/hidden', async function() {
    // The clear search button is hidden initially.
    assertFalse(isVisible(clearSearch));

    // The clear search button is shown when an input event fired.
    const searchEventPromise =
        eventToPromise('search-finished-for-testing', app);
    searchBoxInput('test');
    await searchEventPromise;
    assertTrue(isVisible(clearSearch));

    // The clear search button is hidden after button clicked.
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
          1,
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
