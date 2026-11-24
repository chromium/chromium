// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {CrCollapseElement, CrExpandButtonElement} from 'chrome://settings/lazy_load.js';
import {AiSettingsSection, GlicAssistantSelection, GlicBrowserProxyImpl, LifetimeBrowserProxyImpl, loadTimeData, OpenWindowProxyImpl, PrefsBrowserProxy, PrefService, resetRouterForTesting, SettingsGlicPageFeaturePrefName as PrefName} from 'chrome://settings/settings.js';
import type {CrButtonElement, SettingsGeicSubpageElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';
// <if expr="not is_chromeos">
import {eventToPromise} from 'chrome://webui-test/test_util.js';
// </if>

import {TestGlicBrowserProxy} from './test_glic_browser_proxy.js';
import {TestLifetimeBrowserProxy} from './test_lifetime_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

suite('GeicSubpage', function() {
  let page: SettingsGeicSubpageElement;
  let prefService: PrefService;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;
  let lifetimeBrowserProxy: TestLifetimeBrowserProxy;
  let glicBrowserProxy: TestGlicBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      showAiPage: true,
      aiSettingsSection: AiSettingsSection.GEMINI_ENTERPRISE,
    });
    resetRouterForTesting();
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
    prefsBrowserProxy = new TestPrefsBrowserProxy([
      {
        key: PrefName.TABSTRIP_BUTTON_ENABLED,
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
      {
        key: PrefName.DEFAULT_TAB_CONTEXT_ENABLED,
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
    ]);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);
    glicBrowserProxy = new TestGlicBrowserProxy();
    GlicBrowserProxyImpl.setInstance(glicBrowserProxy);
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

    test('IntroSectionAlwaysVisible', () => {
      const introSection =
          page.shadowRoot.querySelector<HTMLElement>('#introSection');
      assertTrue(!!introSection);
      assertTrue(isVisible(introSection));
      const introHeading =
          page.shadowRoot.querySelector<HTMLElement>('#introHeading');
      const introDescription =
          page.shadowRoot.querySelector<HTMLElement>('#introDescription');
      assertTrue(!!introHeading);
      assertTrue(!!introDescription);
      assertEquals(
          page.i18n('geicIntroHeading'), introHeading.textContent.trim());
      assertEquals(
          page.i18n('geicIntroDescription'),
          introDescription.textContent.trim());
    });

    test('DevicePolicyRowHiddenByDefault', () => {
      assertFalse(
          !!page.shadowRoot.querySelector<HTMLElement>('#devicePolicySection'));
      assertFalse(
          !!page.shadowRoot.querySelector<HTMLElement>('#devicePolicyRow'));
      assertFalse(!!page.shadowRoot.querySelector<HTMLElement>(
          '#devicePolicyConsider'));
      assertFalse(!!page.shadowRoot.querySelector<CrButtonElement>(
          '#devicePolicyButton'));
    });

    test('DevicePolicyConsiderBulletsShown', async () => {
      await createPage(/*showDevicePolicyToggle=*/ true);

      const label =
          page.shadowRoot.querySelector<HTMLElement>('#devicePolicyLabel');
      const sublabel =
          page.shadowRoot.querySelector<HTMLElement>('#devicePolicySublabel');
      const button =
          page.shadowRoot.querySelector<CrButtonElement>('#devicePolicyButton');
      assertTrue(!!label);
      assertTrue(!!sublabel);
      assertTrue(!!button);
      assertEquals(
          page.i18n('geicDevicePolicyToggle'), label.textContent.trim());
      assertEquals(
          page.i18n('geicDevicePolicyToggleSublabel'),
          sublabel.textContent.trim());
      assertEquals(
          page.i18n('geicDevicePolicyButton'), button.textContent.trim());

      const consider =
          page.shadowRoot.querySelector<HTMLElement>('#devicePolicyConsider');
      assertTrue(!!consider);
      assertTrue(isVisible(consider));
      assertEquals(
          page.i18n('columnHeadingConsider'),
          consider.querySelector('.description-header')?.textContent.trim());
      const bullets = consider.querySelectorAll('ul.icon-bulleted-list > li');
      assertEquals(3, bullets.length);
      assertEquals(
          'settings20:draft-spark',
          bullets[0]!.querySelector('cr-icon')?.getAttribute('icon'));
      assertEquals(
          'settings20:shield-spark',
          bullets[1]!.querySelector('cr-icon')?.getAttribute('icon'));
      assertEquals(
          'settings20:chat-spark',
          bullets[2]!.querySelector('cr-icon')?.getAttribute('icon'));
      assertEquals(
          page.i18n('geicDevicePolicyConsider1'),
          bullets[0]!.querySelector('.secondary')?.textContent.trim());
      assertEquals(
          page.i18n('geicDevicePolicyConsider2'),
          bullets[1]!.querySelector('.secondary')?.textContent.trim());
      assertEquals(
          page.i18n('geicSwitchConsiderChatHistory'),
          bullets[2]!.querySelector('.secondary')?.textContent.trim());
    });

    test('DevicePolicyButtonDisablesGeicAndRestarts', async () => {
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

      // The selection is switched before the restart.
      const calls: string[] = [];
      const switched =
          glicBrowserProxy.whenCalled('switchAssistantSelection')
              .then(selection => {
                calls.push('switchAssistantSelection');
                return selection;
              });
      const restarted = lifetimeBrowserProxy.whenCalled('restart').then(
          () => calls.push('restart'));

      button.click();
      await microtasksFinished();
      assertTrue(button.disabled);
      // <if expr="not is_chromeos">
      await lifetimeBrowserProxy.whenCalled('shouldShowRelaunchDialog');
      // </if>
      const [selection] = await Promise.all([switched, restarted]);
      assertDeepEquals(['switchAssistantSelection', 'restart'], calls);
      assertEquals(0, lifetimeBrowserProxy.getCallCount('relaunch'));
      assertEquals(GlicAssistantSelection.CONSUMER, selection);
      assertTrue(button.disabled);
    });

    // <if expr="not is_chromeos">
    test('DevicePolicyButtonConfirmDialogRestarts', async () => {
      lifetimeBrowserProxy.setShouldShowRelaunchConfirmationDialog(true);
      await createPage(/*showDevicePolicyToggle=*/ true);

      const button = page.shadowRoot.querySelector<CrButtonElement>(
          '#devicePolicyButton');
      assertTrue(!!button);
      button.click();
      await lifetimeBrowserProxy.whenCalled('shouldShowRelaunchDialog');
      await microtasksFinished();

      const dialog =
          page.shadowRoot.querySelector('relaunch-confirmation-dialog');
      assertTrue(!!dialog);
      // Not switched until the restart is confirmed.
      assertEquals(
          0, glicBrowserProxy.getCallCount('switchAssistantSelection'));

      dialog.$.confirm.click();
      assertEquals(
          GlicAssistantSelection.CONSUMER,
          await glicBrowserProxy.whenCalled('switchAssistantSelection'));
      await lifetimeBrowserProxy.whenCalled('restart');
      assertEquals(0, lifetimeBrowserProxy.getCallCount('relaunch'));
      await microtasksFinished();
      assertTrue(button.disabled);
    });

    test('DevicePolicyButtonCancelDialogKeepsSelection', async () => {
      lifetimeBrowserProxy.setShouldShowRelaunchConfirmationDialog(true);
      await createPage(/*showDevicePolicyToggle=*/ true);

      const button =
          page.shadowRoot.querySelector<CrButtonElement>('#devicePolicyButton');
      assertTrue(!!button);
      button.click();
      await lifetimeBrowserProxy.whenCalled('shouldShowRelaunchDialog');
      await microtasksFinished();

      const dialog =
          page.shadowRoot.querySelector('relaunch-confirmation-dialog');
      assertTrue(!!dialog);
      dialog.$.cancel.click();
      await eventToPromise('close', dialog);
      await microtasksFinished();

      assertEquals(
          0, glicBrowserProxy.getCallCount('switchAssistantSelection'));
      assertEquals(0, lifetimeBrowserProxy.getCallCount('restart'));
      assertFalse(button.disabled);
    });
    // </if>
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
