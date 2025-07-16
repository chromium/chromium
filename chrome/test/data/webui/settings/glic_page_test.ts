// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {AiPageActions} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData, MetricsBrowserProxyImpl, resetRouterForTesting, Router, routes} from 'chrome://settings/settings.js';
import type {SettingsGlicPageElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('GlicPage', function() {
  let page: SettingsGlicPageElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      showAiPage: true,
      showGlicSettings: true,
      glicDisallowedByAdmin: false,
    });
    resetRouterForTesting();

    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    const settingsPrefs = document.createElement('settings-prefs');
    document.body.appendChild(settingsPrefs);

    await CrSettingsPrefs.initialized;

    Router.getInstance().navigateTo(routes.AI);
    page = document.createElement('settings-glic-page');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
  });

  suite('Default', () => {
    test('ClickGlicRowInGlicSection', () => {
      assertTrue(isVisible(page.$.glicLinkRow));

      page.$.glicLinkRow.click();
      assertEquals(
          routes.GEMINI.path, Router.getInstance().getCurrentRoute().path);
    });

    test('settingsPageLearnMoreHidden', () => {
      // No url, so the element should be hidden.
      assertFalse(isVisible(page.$.learnMoreLabel));
    });
  });

  suite('HeaderLearnMoreEnabled', () => {
    test('settingsPageLearnMoreShown', async () => {
      assertEquals('https://google.com/', page.$.learnMoreLabel.href);

      page.$.learnMoreLabel.click();
      assertEquals(
          AiPageActions.GLIC_COLLAPSED_LEARN_MORE_CLICKED,
          await metricsBrowserProxy.whenCalled('recordAction'));
    });
  });
});
