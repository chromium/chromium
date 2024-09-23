// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {SettingsPrivacyGuidePageElement} from 'chrome://settings/lazy_load.js';
import {PrivacyGuideStep} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, MetricsBrowserProxyImpl, PrivacyGuideStepsEligibleAndReached, Router, routes, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertTrue, assertNotReached} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createPrivacyGuidePageForTest, clickNextOnWelcomeStep, setParametersForCookiesStep, setParametersForHistorySyncStep, setParametersForSafeBrowsingStep, setupPrivacyGuidePageForTest, setupSync, shouldShowCookiesCard, shouldShowHistorySyncCard, shouldShowSafeBrowsingCard} from './privacy_guide_test_util.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on

const privacyGuideStepToEligibleReachedValueMap: Map<PrivacyGuideStep, {
  eligible: PrivacyGuideStepsEligibleAndReached,
  reached: PrivacyGuideStepsEligibleAndReached,
}> =
    new Map([
      [
        PrivacyGuideStep.HISTORY_SYNC,
        {
          eligible: PrivacyGuideStepsEligibleAndReached.HISTORY_SYNC_ELIGIBLE,
          reached: PrivacyGuideStepsEligibleAndReached.HISTORY_SYNC_REACHED,
        },
      ],
      [
        PrivacyGuideStep.SAFE_BROWSING,
        {
          eligible: PrivacyGuideStepsEligibleAndReached.SAFE_BROWSING_ELIGIBLE,
          reached: PrivacyGuideStepsEligibleAndReached.SAFE_BROWSING_REACHED,
        },
      ],
      [
        PrivacyGuideStep.COOKIES,
        {
          eligible: PrivacyGuideStepsEligibleAndReached.COOKIES_ELIGIBLE,
          reached: PrivacyGuideStepsEligibleAndReached.COOKIES_REACHED,
        },
      ],
    ]);

function shouldStepBeShown(
    page: SettingsPrivacyGuidePageElement,
    syncBrowserProxy: TestSyncBrowserProxy, step: PrivacyGuideStep): boolean {
  switch (step) {
    case PrivacyGuideStep.HISTORY_SYNC:
      return shouldShowHistorySyncCard(syncBrowserProxy);
    case PrivacyGuideStep.SAFE_BROWSING:
      return shouldShowSafeBrowsingCard(page);
    case PrivacyGuideStep.COOKIES:
      return shouldShowCookiesCard(page);
    default:
      assertNotReached('Unsupported step type is checking if should be shown.');
  }
}

function setParametersForStep(
    page: SettingsPrivacyGuidePageElement,
    syncBrowserProxy: TestSyncBrowserProxy, step: PrivacyGuideStep,
    isEligible: boolean): void {
  switch (step) {
    case PrivacyGuideStep.HISTORY_SYNC:
      return setParametersForHistorySyncStep(syncBrowserProxy, isEligible);
    case PrivacyGuideStep.SAFE_BROWSING:
      return setParametersForSafeBrowsingStep(page, isEligible);
    case PrivacyGuideStep.COOKIES:
      return setParametersForCookiesStep(page, isEligible);
    default:
      assertNotReached('Unsupported step type is setting parameters.');
  }
}

function isSetEqual(expectedSet: Set<number>, actualSet: Set<number>): boolean {
  return expectedSet.size === actualSet.size &&
      (new Set([...expectedSet, ...actualSet])).size === expectedSet.size;
}

async function getPromiseArguments(
    testMetricsBrowserProxy: TestMetricsBrowserProxy): Promise<Set<number>> {
  await testMetricsBrowserProxy.whenCalled(
      'recordPrivacyGuideStepsEligibleAndReachedHistogram');

  return new Set(testMetricsBrowserProxy.getArgs(
      'recordPrivacyGuideStepsEligibleAndReachedHistogram'));
}

suite('PrivacyGuideEligibleReachedMetrics', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(testSetup);

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  function testSetup(): Promise<void> {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: true,
      syncAllDataTypes: true,
      typedUrlsSynced: true,
    });
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    page = createPrivacyGuidePageForTest(settingsPrefs);
    setupPrivacyGuidePageForTest(page, syncBrowserProxy);

    return flushTasks();
  }

  test('recordStepsAreEligibleReached', async function() {
    const optionalSteps: PrivacyGuideStep[] = [];
    optionalSteps.push(PrivacyGuideStep.HISTORY_SYNC);
    optionalSteps.push(PrivacyGuideStep.SAFE_BROWSING);
    if (!loadTimeData.getBoolean('is3pcdCookieSettingsRedesignEnabled')) {
      optionalSteps.push(PrivacyGuideStep.COOKIES);
    }

    const masks: number[] = [];
    for (let i = 0; i < optionalSteps.length; i++) {
      masks.push(2 ** i);
    }

    // Each optional step can be either eligible or not eligible to be shown.
    // To test all possible permutations of steps a binary vector and bit
    // masking is used. i-th element of the vector represents eligibility of the
    // i-th step in optionalSteps array.
    for (let testCase = 0; testCase < 2 ** optionalSteps.length; testCase++) {
      Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
      await flushTasks();
      await testSetup();

      const expectedArguments = new Set<number>();
      expectedArguments.add(PrivacyGuideStepsEligibleAndReached.MSBB_ELIGIBLE);

      optionalSteps.forEach((step, index) => {
        const isStepEligible = !!(testCase & masks[index]!);
        setParametersForStep(page, syncBrowserProxy, step, isStepEligible);
        if (isStepEligible) {
          expectedArguments.add(
              privacyGuideStepToEligibleReachedValueMap.get(step)!.eligible);
        }
      });

      expectedArguments.add(
          PrivacyGuideStepsEligibleAndReached.COMPLETION_ELIGIBLE);

      await clickNextOnWelcomeStep(page);
      expectedArguments.add(PrivacyGuideStepsEligibleAndReached.MSBB_REACHED);

      assertTrue(
          isSetEqual(
              expectedArguments,
              await getPromiseArguments(testMetricsBrowserProxy)),
          'Sets differ for the step: MSBB_REACHED');

      const nextButtonElementOnMSBBStep =
          page.shadowRoot!.querySelector<HTMLElement>('#nextButton');
      assertTrue(!!nextButtonElementOnMSBBStep);
      nextButtonElementOnMSBBStep.click();

      for (const step of optionalSteps) {
        if (!shouldStepBeShown(page, syncBrowserProxy, step)) {
          continue;
        }

        expectedArguments.add(
            privacyGuideStepToEligibleReachedValueMap.get(step)!.reached);

        assertTrue(
            isSetEqual(
                expectedArguments,
                await getPromiseArguments(testMetricsBrowserProxy)),
            'Sets differ for the step: ' + step);

        const nextButtonElementOnStep =
            page.shadowRoot!.querySelector<HTMLElement>('#nextButton');
        assertTrue(!!nextButtonElementOnStep);
        nextButtonElementOnStep.click();
      }

      expectedArguments.add(
          PrivacyGuideStepsEligibleAndReached.COMPLETION_REACHED);

      assertTrue(
          isSetEqual(
              expectedArguments,
              await getPromiseArguments(testMetricsBrowserProxy)),
          'Sets differ for the step: COMPLETION_REACHED');
    }
  });
});
