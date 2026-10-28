// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {loadTimeData, OpenWindowProxyImpl, PrefService, resetRouterForTesting} from 'chrome://settings/settings.js';
import type {SettingsGeicSubpageElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('GeicSubpage', function() {
  let page: SettingsGeicSubpageElement;
  let prefService: PrefService;
  let openWindowProxy: TestOpenWindowProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      showAiPage: true,
      showGeicSettings: true,
    });
    resetRouterForTesting();
    prefService = PrefService.getInstance();
    return prefService.whenInitialized();
  });

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    page = document.createElement('settings-geic-subpage');
    document.body.appendChild(page);
    await microtasksFinished();
  });

  suite('Default', () => {
    test('DefaultTabAccessToggleLearnMoreLink', async () => {
      const toggle = page.shadowRoot.querySelector<SettingsToggleButtonElement>(
          '#defaultTabAccessToggle');
      assertTrue(!!toggle);

      const link = toggle.shadowRoot.querySelector('a');
      assertTrue(!!link);
      link.click();

      const url = await openWindowProxy.whenCalled('openUrl');
      assertTrue(
          url.startsWith(
              'https://docs.cloud.google.com/gemini/enterprise/docs/get-started'),
          url);
      assertEquals(page.i18n('geicDefaultTabAccessToggleLearnMoreUrl'), url);
    });
  });

  suite('LearnMoreEnabled', () => {
    test('DefaultTabAccessToggleLearnMoreOverride', async () => {
      const toggle = page.shadowRoot.querySelector<SettingsToggleButtonElement>(
          '#defaultTabAccessToggle');
      assertTrue(!!toggle);

      const link = toggle.shadowRoot.querySelector('a');
      assertTrue(!!link);
      link.click();

      const url = await openWindowProxy.whenCalled('openUrl');
      assertEquals('https://google.com/?hl=en-US', url);
    });
  });
});
