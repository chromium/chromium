// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {CrCollapseElement, CrExpandButtonElement} from 'chrome://settings/lazy_load.js';
import {AiSettingsSection, GlicAssistantSelection, LifetimeBrowserProxyImpl, loadTimeData, OpenWindowProxyImpl, PrefService, resetRouterForTesting, SettingsGlicPageFeaturePrefName as PrefName} from 'chrome://settings/settings.js';
import type {CrButtonElement, SettingsGeicSubpageElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestLifetimeBrowserProxy} from './test_lifetime_browser_proxy.js';

suite('GeicSubpage', function() {
  let page: SettingsGeicSubpageElement;
  let prefService: PrefService;
  let openWindowProxy: TestOpenWindowProxy;
  let lifetimeBrowserProxy: TestLifetimeBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      showAiPage: true,
      aiSettingsSection: AiSettingsSection.GEMINI_ENTERPRISE,
    });
    resetRouterForTesting();
    prefService = PrefService.getInstance();
    return prefService.whenInitialized();
  });

  async function createPage(showDevicePolicyToggle: boolean = false) {
    loadTimeData.overrideValues({
      showGeicDevicePolicyToggle: showDevicePolicyToggle,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-geic-subpage');
    document.body.appendChild(page);
    await microtasksFinished();
  }

  setup(async function() {
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);
    await prefService.setPrefValue(
        'glic.assistant_selection', GlicAssistantSelection.AUTO);
    await createPage();
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
    // Each test awaits the initial pref write to the browser and then toggles
    // once, so a late pref update from the browser can't race the click.
    test('TabstripButtonTogglePref', async () => {
      const toggle = page.shadowRoot.querySelector<SettingsToggleButtonElement>(
          '#tabstripButtonToggle');
      assertTrue(!!toggle);

      assertTrue(
          await prefService.setPrefValue(PrefName.TABSTRIP_BUTTON_ENABLED, true));
      await microtasksFinished();
      assertTrue(toggle.checked);

      toggle.$.control.click();
      await microtasksFinished();
      assertFalse(
          prefService.getPref<boolean>(PrefName.TABSTRIP_BUTTON_ENABLED).value);
      assertFalse(toggle.checked);
    });

    test('DefaultTabAccessTogglePref', async () => {
      const toggle = page.shadowRoot.querySelector<SettingsToggleButtonElement>(
          '#defaultTabAccessToggle');
      assertTrue(!!toggle);

      assertTrue(await prefService.setPrefValue(
          PrefName.DEFAULT_TAB_CONTEXT_ENABLED, false));
      await microtasksFinished();
      assertFalse(toggle.checked);

      toggle.$.control.click();
      await microtasksFinished();
      assertTrue(
          prefService.getPref<boolean>(PrefName.DEFAULT_TAB_CONTEXT_ENABLED)
              .value);
      assertTrue(toggle.checked);
    });

    test('DefaultTabAccessExpand', async () => {
      const toggle = page.shadowRoot.querySelector<SettingsToggleButtonElement>(
          '#defaultTabAccessToggle');
      const expandButton = page.shadowRoot.querySelector<CrExpandButtonElement>(
          '#defaultTabAccessExpandButton');
      const infoCollapse = page.shadowRoot.querySelector<CrCollapseElement>(
          '#defaultTabAccessInfoCollapse');
      assertTrue(!!toggle);
      assertTrue(!!expandButton);
      assertTrue(!!infoCollapse);
      assertTrue(await prefService.setPrefValue(
          PrefName.DEFAULT_TAB_CONTEXT_ENABLED, false));
      await microtasksFinished();
      assertFalse(infoCollapse.opened);

      // Clicking the toggle's host element expands and collapses the info
      // without changing the pref.
      toggle.click();
      await microtasksFinished();
      assertTrue(infoCollapse.opened);
      assertFalse(
          prefService.getPref<boolean>(PrefName.DEFAULT_TAB_CONTEXT_ENABLED)
              .value);

      toggle.click();
      await microtasksFinished();
      assertFalse(infoCollapse.opened);
      assertFalse(
          prefService.getPref<boolean>(PrefName.DEFAULT_TAB_CONTEXT_ENABLED)
              .value);

      // Clicking the expand button also expands and collapses the info without
      // changing the pref.
      expandButton.click();
      await microtasksFinished();
      assertTrue(infoCollapse.opened);
      assertFalse(
          prefService.getPref<boolean>(PrefName.DEFAULT_TAB_CONTEXT_ENABLED)
              .value);

      expandButton.click();
      await microtasksFinished();
      assertFalse(infoCollapse.opened);
      assertFalse(
          prefService.getPref<boolean>(PrefName.DEFAULT_TAB_CONTEXT_ENABLED)
              .value);
    });

    test('DevicePolicyRowHiddenByDefault', () => {
      assertFalse(
          !!page.shadowRoot.querySelector<HTMLElement>('#devicePolicyRow'));
      assertFalse(!!page.shadowRoot.querySelector<CrButtonElement>(
          '#devicePolicyButton'));
    });

    test('DevicePolicyButtonDisablesGeicAndRelaunches', async () => {
      await createPage(/*showDevicePolicyToggle=*/ true);

      const row =
          page.shadowRoot.querySelector<HTMLElement>('#devicePolicyRow');
      const button = page.shadowRoot.querySelector<CrButtonElement>(
          '#devicePolicyButton');
      assertTrue(!!row);
      assertTrue(!!button);
      assertTrue(isVisible(row));
      assertTrue(isVisible(button));
      assertFalse(button.disabled);
      assertEquals(
          GlicAssistantSelection.AUTO,
          prefService.getPref<number>('glic.assistant_selection').value);

      button.click();
      await microtasksFinished();
      assertTrue(button.disabled);
      // <if expr="not is_chromeos">
      await lifetimeBrowserProxy.whenCalled('shouldShowRelaunchDialog');
      // </if>
      await lifetimeBrowserProxy.whenCalled('relaunch');
      assertEquals(
          GlicAssistantSelection.CONSUMER,
          prefService.getPref<number>('glic.assistant_selection').value);

      page.onRelaunchDialogClose(new Event('close'));
      await microtasksFinished();
      assertFalse(button.disabled);
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
