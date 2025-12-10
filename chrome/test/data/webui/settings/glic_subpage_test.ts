// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {AiPageActions, type CrCollapseElement} from 'chrome://settings/lazy_load.js';
import type {SettingsGlicSubpageElement, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, GlicBrowserProxyImpl, loadTimeData, MetricsBrowserProxyImpl, OpenWindowProxyImpl, resetRouterForTesting, SettingsGlicPageFeaturePrefName as PrefName} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestGlicBrowserProxy} from './test_glic_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

// Note - if adding tests related to the shortcut control, use
// glic_page_focus_test.ts instead. That test suite is an interactive_ui_test
// which correctly deals with focus. The shortcut control relies internally on
// focus events to work so using this suite results in flaky tests.
suite('GlicSubpage', function() {
  let page: SettingsGlicSubpageElement;
  let settingsPrefs: SettingsPrefsElement;
  let glicBrowserProxy: TestGlicBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  async function createGlicPage(initialShortcut: string) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    glicBrowserProxy = new TestGlicBrowserProxy();
    glicBrowserProxy.setGlicShortcutResponse(initialShortcut);
    GlicBrowserProxyImpl.setInstance(glicBrowserProxy);

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    page = document.createElement('settings-glic-subpage');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);

    await flushTasks();
    disableAnimationForCrCollapseElements();
  }

  function disableAnimationForCrCollapseElements() {
    const collapseElements = page.shadowRoot!.querySelectorAll('cr-collapse');

    for (const collapseElement of collapseElements) {
      collapseElement.noAnimation = true;
    }
  }

  function $<T extends HTMLElement = HTMLElement>(id: string): T|null {
    return page.shadowRoot!.querySelector<T>(`#${id}`);
  }

  async function assertFeatureInteractionMetrics(action: AiPageActions) {
    assertEquals(action, await metricsBrowserProxy.whenCalled('recordAction'));
  }

  async function clickToggle() {
    const launcherToggle = $<SettingsToggleButtonElement>('launcherToggle');
    assertTrue(!!launcherToggle);
    launcherToggle.$.control.click();
    await flushTasks();
  }

  function clickToggleRow() {
    const launcherToggle = $<SettingsToggleButtonElement>('launcherToggle');
    assertTrue(!!launcherToggle);
    launcherToggle.click();
  }

  function setDisallowedByAdminAndSimulateUpdate(disallowed: boolean) {
    glicBrowserProxy.setDisallowedByAdmin(disallowed);
    // Simulate the update we would get if the browser detected a change.
    webUIListenerCallback('glic-disallowed-by-admin-changed', disallowed);
    return flushTasks();
  }

  async function verifyUserAction(userAction: string) {
    const userActions = await metricsBrowserProxy.getArgs('recordAction');
    assertEquals(1, userActions.length);
    assertTrue(userActions.includes(userAction));
    metricsBrowserProxy.reset();
  }

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    loadTimeData.overrideValues({
      showAiPage: true,
      showGlicSettings: true,
      glicDisallowedByAdmin: false,
    });
    resetRouterForTesting();
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    return createGlicPage(/*initialShortcut=*/ '⌃A');
  });

  suite('Default', () => {
    test('LauncherToggleEnabled', () => {
      page.setPrefValue(PrefName.LAUNCHER_ENABLED, true);

      assertTrue($<SettingsToggleButtonElement>('launcherToggle')!.checked);
    });

    test('LauncherToggleDisabled', () => {
      page.setPrefValue(PrefName.LAUNCHER_ENABLED, false);

      assertFalse($<SettingsToggleButtonElement>('launcherToggle')!.checked);
    });

    for (const clickType of [clickToggle, clickToggleRow]) {
      const clickTypeName = clickType.name.replace('click', '');
      test('Launcher' + clickTypeName + 'Change', async () => {
        page.setPrefValue(PrefName.LAUNCHER_ENABLED, false);

        const launcherToggle =
            $<SettingsToggleButtonElement>('launcherToggle')!;

        await clickType();
        assertTrue(page.getPref(PrefName.LAUNCHER_ENABLED).value);
        assertTrue(launcherToggle.checked);
        assertEquals(
            1, glicBrowserProxy.getCallCount('setGlicOsLauncherEnabled'));
        glicBrowserProxy.reset();

        await clickType();
        assertFalse(page.getPref(PrefName.LAUNCHER_ENABLED).value);
        assertFalse(launcherToggle.checked);
        assertEquals(
            1, glicBrowserProxy.getCallCount('setGlicOsLauncherEnabled'));
        glicBrowserProxy.reset();
      });

      // Test that the keyboard shortcut is collapsed/invisible when the
      // launcher is disabled and shown when the launcher is enabled.
      test('KeyboardShortcutVisibility' + clickTypeName, async () => {
        const mainShortcutSettingId = 'mainShortcutSetting';

        // The pref starts off disabled, the keyboard shortcut row should be
        // hidden.
        page.setPrefValue(PrefName.LAUNCHER_ENABLED, false);
        await flushTasks();
        assertFalse(isVisible($(mainShortcutSettingId)));

        // Enable using the launcher toggle, the row should show.
        await clickType();
        assertTrue(page.getPref(PrefName.LAUNCHER_ENABLED).value);
        await flushTasks();
        assertTrue(isVisible($(mainShortcutSettingId)));

        // Disable using the launcher toggle, the row should hide.
        await clickType();
        assertFalse(page.getPref(PrefName.LAUNCHER_ENABLED).value);
        await flushTasks();
        assertFalse(isVisible($(mainShortcutSettingId)));

        // Enable via pref, the row should show.
        page.setPrefValue(PrefName.LAUNCHER_ENABLED, true);
        await flushTasks();
        assertTrue(isVisible($(mainShortcutSettingId)));
      });
    }

    test('GeolocationToggleEnabled', () => {
      page.setPrefValue(PrefName.GEOLOCATION_ENABLED, true);

      assertTrue($<SettingsToggleButtonElement>('geolocationToggle')!.checked);
    });

    test('GeolocationToggleDisabled', () => {
      page.setPrefValue(PrefName.GEOLOCATION_ENABLED, false);

      assertFalse($<SettingsToggleButtonElement>('geolocationToggle')!.checked);
    });

    test('GeolocationToggleChange', () => {
      page.setPrefValue(PrefName.GEOLOCATION_ENABLED, false);

      const geolocationToggle =
          $<SettingsToggleButtonElement>('geolocationToggle')!;
      assertTrue(!!geolocationToggle);

      geolocationToggle.click();
      assertTrue(page.getPref(PrefName.GEOLOCATION_ENABLED).value);
      assertTrue(geolocationToggle.checked);

      geolocationToggle.click();
      assertFalse(page.getPref(PrefName.GEOLOCATION_ENABLED).value);
      assertFalse(geolocationToggle.checked);
    });

    test('MicrophoneToggleEnabled', () => {
      page.setPrefValue(PrefName.MICROPHONE_ENABLED, true);

      assertTrue($<SettingsToggleButtonElement>('microphoneToggle')!.checked);
    });

    test('MicrophoneToggleDisabled', () => {
      page.setPrefValue(PrefName.MICROPHONE_ENABLED, false);

      assertFalse($<SettingsToggleButtonElement>('microphoneToggle')!.checked);
    });

    test('MicrophoneToggleChange', () => {
      page.setPrefValue(PrefName.MICROPHONE_ENABLED, false);

      const microphoneToggle =
          $<SettingsToggleButtonElement>('microphoneToggle')!;
      assertTrue(!!microphoneToggle);

      microphoneToggle.click();
      assertTrue(page.getPref(PrefName.MICROPHONE_ENABLED).value);
      assertTrue(microphoneToggle.checked);

      microphoneToggle.click();
      assertFalse(page.getPref(PrefName.MICROPHONE_ENABLED).value);
      assertFalse(microphoneToggle.checked);
    });

    test('TabContextToggleEnabled', () => {
      page.setPrefValue(PrefName.TAB_CONTEXT_ENABLED, true);

      assertTrue($<SettingsToggleButtonElement>('tabAccessToggle')!.checked);
    });

    test('TabContextToggleDisabled', () => {
      page.setPrefValue(PrefName.TAB_CONTEXT_ENABLED, false);

      assertFalse($<SettingsToggleButtonElement>('tabAccessToggle')!.checked);
    });

    test('TabContextToggleChange', async () => {
      page.setPrefValue(PrefName.TAB_CONTEXT_ENABLED, false);

      const tabAccessToggle = $<SettingsToggleButtonElement>('tabAccessToggle');
      assertTrue(!!tabAccessToggle);

      tabAccessToggle.$.control.click();
      await flushTasks();
      assertTrue(page.getPref(PrefName.TAB_CONTEXT_ENABLED).value);
      assertTrue(tabAccessToggle.checked);

      tabAccessToggle.$.control.click();
      await flushTasks();
      assertFalse(page.getPref(PrefName.TAB_CONTEXT_ENABLED).value);
      assertFalse(tabAccessToggle.checked);
    });

    test('TabContextExpand', async () => {
      const tabAccessToggle =
          $<SettingsToggleButtonElement>('tabAccessToggle')!;
      const infoCard = $<CrCollapseElement>('tabAccessInfoCollapse')!;

      assertFalse(infoCard.opened);

      // Clicking the host element of the toggle button opens the info card but
      // does not change the pref.
      tabAccessToggle.click();
      await flushTasks();
      assertTrue(infoCard.opened);
      assertFalse(page.getPref(PrefName.TAB_CONTEXT_ENABLED).value);

      // Clicking the host element again collapses the info card.
      tabAccessToggle.click();
      await flushTasks();
      assertFalse(infoCard.opened);
      assertFalse(page.getPref(PrefName.TAB_CONTEXT_ENABLED).value);

      // Toggling the setting to on opens the info card.
      tabAccessToggle.$.control.click();
      await flushTasks();
      assertTrue(page.getPref(PrefName.TAB_CONTEXT_ENABLED).value);
      assertTrue(infoCard.opened);

      // Toggling the setting off closes the info card.
      tabAccessToggle.$.control.click();
      await flushTasks();
      assertFalse(page.getPref(PrefName.TAB_CONTEXT_ENABLED).value);
      assertFalse(infoCard.opened);

      // Toggling the setting to on while the info card is open leaves it open.
      tabAccessToggle.click();
      await flushTasks();
      assertTrue(infoCard.opened);
      tabAccessToggle.$.control.click();
      await flushTasks();
      assertTrue(page.getPref(PrefName.TAB_CONTEXT_ENABLED).value);
      assertTrue(infoCard.opened);

      // Toggling the setting to off while the info card is closed leaves it
      // closed.
      tabAccessToggle.click();
      await flushTasks();
      assertFalse(infoCard.opened);
      tabAccessToggle.$.control.click();
      await flushTasks();
      assertFalse(page.getPref(PrefName.TAB_CONTEXT_ENABLED).value);
      assertFalse(infoCard.opened);
    });

    // Ensure the page reacts appropriately to the enterprise policy pref being
    // flipped off and back on.
    test('DisabledByPolicy', async () => {
      page.setPrefValue(PrefName.LAUNCHER_ENABLED, true);
      page.setPrefValue(PrefName.GEOLOCATION_ENABLED, true);
      page.setPrefValue(PrefName.MICROPHONE_ENABLED, true);
      page.setPrefValue(PrefName.TAB_CONTEXT_ENABLED, true);
      page.setPrefValue(PrefName.TABSTRIP_BUTTON_ENABLED, true);

      const shortcutInputSelector = 'mainShortcutSetting .shortcut-input';

      // Page starts off with policy enabled. The shortcut editor, info card
      // expand, and activity button are all present.
      assertTrue(isVisible($(shortcutInputSelector)));
      assertTrue(!!$('activityButton'));
      assertTrue(!!$('tabAccessExpandButton'));
      assertTrue(!!$('tabAccessInfoCollapse'));

      // Toggles should all have values from the real pref and be enabled.
      let toggles = page.shadowRoot!.querySelectorAll(
          'settings-toggle-button[checked]:not([disabled])');
      assertEquals(6, toggles.length);

      await setDisallowedByAdminAndSimulateUpdate(true);

      // Now that the policy is disabled, the shortcut edit, info card expand,
      // and activity button should be removed. Toggles should all show "off"
      // and be disabled.
      assertFalse(!!$(shortcutInputSelector));
      assertFalse(!!$('activityButton'));
      assertFalse(!!$('tabAccessExpandButton'));
      assertFalse(!!$('tabAccessInfoCollapse'));

      toggles = page.shadowRoot!.querySelectorAll(
          'settings-toggle-button:not([checked])[disabled]');
      assertEquals(6, toggles.length);

      // Re-enable the policy, the page should go back to the initial state.
      await setDisallowedByAdminAndSimulateUpdate(false);

      assertTrue(isVisible($(shortcutInputSelector)));
      assertTrue(!!$('activityButton'));
      assertTrue(!!$('tabAccessExpandButton'));
      assertTrue(!!$('tabAccessInfoCollapse'));

      toggles = page.shadowRoot!.querySelectorAll(
          'settings-toggle-button[checked]:not([disabled])');
      assertEquals(6, toggles.length);
    });

    test('ManageActivityRow', async () => {
      page.setPrefValue(PrefName.GEOLOCATION_ENABLED, false);

      const activityButton = $<HTMLElement>('activityButton');
      assertTrue(!!activityButton);

      activityButton.click();
      const url = await openWindowProxy.whenCalled('openUrl');
      assertEquals(page.i18n('glicActivityButtonUrl'), url);
    });

    // Ensure that the info collapse is initialized correctly when the tab
    // context pref is enabled when the page is created.
    test('InfoCollapseInitializiedOpen', async () => {
      // Clear and re-create a new page rather than using the one initialized in
      // setup().
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      page = document.createElement('settings-glic-subpage');
      page.prefs = settingsPrefs.prefs;
      page.setPrefValue(PrefName.TAB_CONTEXT_ENABLED, true);
      document.body.appendChild(page);

      await flushTasks();

      const infoCard = $<CrCollapseElement>('tabAccessInfoCollapse');
      assertTrue(!!infoCard);
      assertTrue(infoCard.opened);
    });

    test('InfoCollapseInitializiedClosed', async () => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      page = document.createElement('settings-glic-subpage');
      page.prefs = settingsPrefs.prefs;
      page.setPrefValue(PrefName.TAB_CONTEXT_ENABLED, false);
      document.body.appendChild(page);

      await flushTasks();

      const infoCard = $<CrCollapseElement>('tabAccessInfoCollapse');
      assertTrue(!!infoCard);
      assertFalse(infoCard.opened);
    });

    test('ClosedCaptionsToggleFeatureDisabled', () => {
      const closedCaptionsToggle =
          $<SettingsToggleButtonElement>('closedCaptionsToggle')!;
      assertFalse(isVisible(closedCaptionsToggle));
    });

    test('DefaultTabContextSettingFeatureDisabled', () => {
      const defaultTabAccessToggle =
          $<SettingsToggleButtonElement>('defaultTabAccessToggle')!;
      assertFalse(isVisible(defaultTabAccessToggle));
    });

    test('tabstripButtonToggleEnabled', () => {
      page.setPrefValue(PrefName.TABSTRIP_BUTTON_ENABLED, true);

      assertTrue(
          $<SettingsToggleButtonElement>('tabstripButtonToggle')!.checked);
    });

    test('tabstripButtonToggleDisabled', () => {
      page.setPrefValue(PrefName.TABSTRIP_BUTTON_ENABLED, false);

      assertFalse(
          $<SettingsToggleButtonElement>('tabstripButtonToggle')!.checked);
    });

    test('tabstripButtonToggleChanged', () => {
      page.setPrefValue(PrefName.TABSTRIP_BUTTON_ENABLED, false);

      const tabstripButtonToggle =
          $<SettingsToggleButtonElement>('tabstripButtonToggle');
      assertTrue(!!tabstripButtonToggle);

      tabstripButtonToggle.click();
      assertTrue(page.getPref(PrefName.TABSTRIP_BUTTON_ENABLED).value);
      assertTrue(tabstripButtonToggle.checked);

      tabstripButtonToggle.click();
      assertFalse(page.getPref(PrefName.TABSTRIP_BUTTON_ENABLED).value);
      assertFalse(tabstripButtonToggle.checked);
    });

    suite('Metrics', () => {
      test('GeolocationToggle', async () => {
        page.setPrefValue(PrefName.GEOLOCATION_ENABLED, false);

        const geolocationToggle =
            $<SettingsToggleButtonElement>('geolocationToggle')!;
        assertTrue(!!geolocationToggle);

        geolocationToggle.click();
        await verifyUserAction('Glic.Settings.Geolocation.Enabled');

        geolocationToggle.click();
        await verifyUserAction('Glic.Settings.Geolocation.Disabled');
      });

      test('MicrophoneToggle', async () => {
        page.setPrefValue(PrefName.MICROPHONE_ENABLED, false);

        const microphoneToggle =
            $<SettingsToggleButtonElement>('microphoneToggle')!;
        assertTrue(!!microphoneToggle);

        microphoneToggle.click();
        await verifyUserAction('Glic.Settings.Microphone.Enabled');

        microphoneToggle.click();
        await verifyUserAction('Glic.Settings.Microphone.Disabled');
      });

      test('TabContextToggle', async () => {
        page.setPrefValue(PrefName.TAB_CONTEXT_ENABLED, false);

        const tabAccessToggle =
            $<SettingsToggleButtonElement>('tabAccessToggle')!;
        assertTrue(!!tabAccessToggle);

        tabAccessToggle.$.control.click();
        await flushTasks();
        await verifyUserAction('Glic.Settings.TabContext.Enabled');


        tabAccessToggle.$.control.click();
        await flushTasks();
        await verifyUserAction('Glic.Settings.TabContext.Disabled');
      });
    });

    test('keyboardShortcutLearnMoreHidden', () => {
      // No url, so the element should be hidden.
      page.setPrefValue(PrefName.LAUNCHER_ENABLED, true);
      assertTrue($<SettingsToggleButtonElement>('launcherToggle')!.checked);
      const learnMoreElement = $('shortcutsLearnMoreLabel');
      assertFalse(isVisible(learnMoreElement));
    });
  });

  suite('LearnMoreEnabled', () => {
    test('keyboardShortcutLearnMoreShown', async () => {
      page.setPrefValue(PrefName.LAUNCHER_ENABLED, true);
      await setDisallowedByAdminAndSimulateUpdate(false);
      assertTrue($<SettingsToggleButtonElement>('launcherToggle')!.checked);

      const learnMoreElement = $<HTMLAnchorElement>('shortcutsLearnMoreLabel');
      assertTrue(!!learnMoreElement);
      assertEquals('https://google.com/', learnMoreElement.href);

      learnMoreElement.click();
      await assertFeatureInteractionMetrics(
          AiPageActions.GLIC_SHORTCUTS_LEARN_MORE_CLICKED);
    });
  });

  suite('LauncherToggleLearnMoreEnabled', () => {
    test('LauncherToggleLearnMoreShown', async () => {
      const learnMoreElement =
          page.shadowRoot!.querySelector<HTMLElement>('#launcherToggle')!
              .shadowRoot!.querySelector<HTMLAnchorElement>('#learn-more');
      assertTrue(!!learnMoreElement);
      assertEquals(learnMoreElement.href, 'https://google.com/');

      learnMoreElement.click();
      await assertFeatureInteractionMetrics(
          AiPageActions.GLIC_SHORTCUTS_LAUNCHER_TOGGLE_LEARN_MORE_CLICKED);
    });
  });

  suite('LocationToggleLearnMoreEnabled', () => {
    test('locationToggleLearnMoreShown', async () => {
      const learnMoreElement =
          page.shadowRoot!.querySelector<HTMLElement>('#geolocationToggle')!
              .shadowRoot!.querySelector<HTMLAnchorElement>('#learn-more');
      assertTrue(!!learnMoreElement);
      assertEquals(learnMoreElement.href, 'https://google.com/');

      learnMoreElement.click();
      await assertFeatureInteractionMetrics(
          AiPageActions.GLIC_SHORTCUTS_LOCATION_TOGGLE_LEARN_MORE_CLICKED);
    });
  });

  suite('ClosedCaptionsToggleEnabled', () => {
    test('ClosedCaptionsToggleFeatureEnabled', () => {
      const closedCaptionsToggle =
          $<SettingsToggleButtonElement>('closedCaptionsToggle')!;
      assertTrue(isVisible(closedCaptionsToggle));
    });

    test('ClosedCaptionsToggleEnabled', () => {
      page.setPrefValue(PrefName.CLOSED_CAPTIONS_ENABLED, true);

      assertTrue(
          $<SettingsToggleButtonElement>('closedCaptionsToggle')!.checked);
    });

    test('ClosedCaptionsToggleDisabled', () => {
      page.setPrefValue(PrefName.CLOSED_CAPTIONS_ENABLED, false);

      assertFalse(
          $<SettingsToggleButtonElement>('closedCaptionsToggle')!.checked);
    });

    test('ClosedCaptionsToggleChanged', async () => {
      page.setPrefValue(PrefName.CLOSED_CAPTIONS_ENABLED, false);

      const closedCaptionsToggle =
          $<SettingsToggleButtonElement>('closedCaptionsToggle')!;
      assertTrue(!!closedCaptionsToggle);

      closedCaptionsToggle.click();
      assertTrue(page.getPref(PrefName.CLOSED_CAPTIONS_ENABLED).value);
      assertTrue(closedCaptionsToggle.checked);
      await verifyUserAction('Glic.Settings.ClosedCaptions.Enabled');

      closedCaptionsToggle.click();
      assertFalse(page.getPref(PrefName.CLOSED_CAPTIONS_ENABLED).value);
      assertFalse(closedCaptionsToggle.checked);
      await verifyUserAction('Glic.Settings.ClosedCaptions.Disabled');
    });
  });

  suite('KeepSidepanelOpenOnNewTabsToggleEnabled', () => {
    test('KeepSidepanelOpenOnNewTabsFeatureEnabled', () => {
      const keepSidepanelOpenOnNewTabsToggle =
          $<SettingsToggleButtonElement>('keepSidepanelOpenOnNewTabsToggle')!;
      assertTrue(isVisible(keepSidepanelOpenOnNewTabsToggle));
    });

    test('KeepSidepanelOpenOnNewTabsToggleEnabled', () => {
      page.setPrefValue(PrefName.KEEP_SIDEPANEL_OPEN_ON_NEW_TABS_ENABLED, true);

      assertTrue(
          $<SettingsToggleButtonElement>(
              'keepSidepanelOpenOnNewTabsToggle')!.checked);
    });

    test('KeepSidepanelOpenOnNewTabsToggleDisabled', () => {
      page.setPrefValue(
          PrefName.KEEP_SIDEPANEL_OPEN_ON_NEW_TABS_ENABLED, false);

      assertFalse(
          $<SettingsToggleButtonElement>(
              'keepSidepanelOpenOnNewTabsToggle')!.checked);
    });

    test('KeepSidepanelOpenOnNewTabsToggleChanged', async () => {
      page.setPrefValue(
          PrefName.KEEP_SIDEPANEL_OPEN_ON_NEW_TABS_ENABLED, false);

      const keepSidepanelOpenOnNewTabsToggle =
          $<SettingsToggleButtonElement>('keepSidepanelOpenOnNewTabsToggle')!;
      assertTrue(!!keepSidepanelOpenOnNewTabsToggle);

      keepSidepanelOpenOnNewTabsToggle.click();
      assertTrue(
          page.getPref(PrefName.KEEP_SIDEPANEL_OPEN_ON_NEW_TABS_ENABLED).value);
      assertTrue(keepSidepanelOpenOnNewTabsToggle.checked);
      await verifyUserAction(
          'Glic.Settings.KeepSidepanelOpenOnNewTabs.Enabled');

      keepSidepanelOpenOnNewTabsToggle.click();
      assertFalse(
          page.getPref(PrefName.KEEP_SIDEPANEL_OPEN_ON_NEW_TABS_ENABLED).value);
      assertFalse(keepSidepanelOpenOnNewTabsToggle.checked);
      await verifyUserAction(
          'Glic.Settings.KeepSidepanelOpenOnNewTabs.Disabled');
    });
  });

  suite('DefaultTabContextSettingFeatureEnabled', () => {
    test('DefaultTabContextSettingFeatureEnabled', () => {
      const defaultTabAccessToggle =
          $<SettingsToggleButtonElement>('defaultTabAccessToggle')!;
      assertTrue(isVisible(defaultTabAccessToggle));
    });

    test('TabContextSettingHidden', () => {
      const tabAccessToggle =
          $<SettingsToggleButtonElement>('tabAccessToggle')!;
      assertFalse(isVisible(tabAccessToggle));
    });

    test('ToggleEnabled', () => {
      page.setPrefValue(PrefName.DEFAULT_TAB_CONTEXT_ENABLED, true);

      assertTrue(
          $<SettingsToggleButtonElement>('defaultTabAccessToggle')!.checked);
    });

    test('ToggleDisabled', () => {
      page.setPrefValue(PrefName.DEFAULT_TAB_CONTEXT_ENABLED, false);

      assertFalse(
          $<SettingsToggleButtonElement>('defaultTabAccessToggle')!.checked);
    });

    test('DefaultTabContextExpand', async () => {
      const defaultTabAccessToggle =
          $<SettingsToggleButtonElement>('defaultTabAccessToggle')!;
      const infoCard = $<CrCollapseElement>('defaultTabAccessInfoCollapse')!;
      page.setPrefValue(PrefName.DEFAULT_TAB_CONTEXT_ENABLED, false);

      assertFalse(infoCard.opened);

      // Clicking the host element of the toggle button opens the info card but
      // does not change the pref.
      defaultTabAccessToggle.click();
      await flushTasks();
      assertTrue(infoCard.opened);
      assertFalse(page.getPref(PrefName.DEFAULT_TAB_CONTEXT_ENABLED).value);

      // Clicking the host element again collapses the info card.
      defaultTabAccessToggle.click();
      await flushTasks();
      assertFalse(infoCard.opened);
      assertFalse(page.getPref(PrefName.DEFAULT_TAB_CONTEXT_ENABLED).value);

      // Toggling the setting to on opens the info card.
      defaultTabAccessToggle.$.control.click();
      await flushTasks();
      assertTrue(page.getPref(PrefName.DEFAULT_TAB_CONTEXT_ENABLED).value);
      assertTrue(infoCard.opened);

      // Toggling the setting off closes the info card.
      defaultTabAccessToggle.$.control.click();
      await flushTasks();
      assertFalse(page.getPref(PrefName.DEFAULT_TAB_CONTEXT_ENABLED).value);
      assertFalse(infoCard.opened);

      // Toggling the setting to on while the info card is open leaves it open.
      defaultTabAccessToggle.click();
      await flushTasks();
      assertTrue(infoCard.opened);
      defaultTabAccessToggle.$.control.click();
      await flushTasks();
      assertTrue(page.getPref(PrefName.DEFAULT_TAB_CONTEXT_ENABLED).value);
      assertTrue(infoCard.opened);

      // Toggling the setting to off while the info card is closed leaves it
      // closed.
      defaultTabAccessToggle.click();
      await flushTasks();
      assertFalse(infoCard.opened);
      defaultTabAccessToggle.$.control.click();
      await flushTasks();
      assertFalse(page.getPref(PrefName.DEFAULT_TAB_CONTEXT_ENABLED).value);
      assertFalse(infoCard.opened);
    });
  });

  suite('WebActuationSettingFeatureEnabled', () => {
    test('WebActuationSettingFeatureEnabled', () => {
      const webActuationToggle =
          $<SettingsToggleButtonElement>('webActuationToggle')!;
      assertTrue(isVisible(webActuationToggle));
    });

    test('ToggleEnabled', () => {
      page.setPrefValue(PrefName.WEB_ACTUATION_ENABLED, true);

      assertTrue($<SettingsToggleButtonElement>('webActuationToggle')!.checked);
    });

    test('ToggleDisabled', () => {
      page.setPrefValue(PrefName.WEB_ACTUATION_ENABLED, false);

      assertFalse(
          $<SettingsToggleButtonElement>('webActuationToggle')!.checked);
    });

    test('WebActuationExpand', async () => {
      const webActuationToggle =
          $<SettingsToggleButtonElement>('webActuationToggle')!;
      const infoCard = $<CrCollapseElement>('webActuationInfoCollapse')!;
      page.setPrefValue(PrefName.WEB_ACTUATION_ENABLED, false);

      assertFalse(infoCard.opened);

      // Clicking the host element of the toggle button expands the info card
      // but does not change the pref.
      webActuationToggle.click();
      await flushTasks();
      assertTrue(infoCard.opened);
      assertFalse(page.getPref(PrefName.WEB_ACTUATION_ENABLED).value);

      // Clicking the host element again collapses the info card.
      webActuationToggle.click();
      await flushTasks();
      assertFalse(infoCard.opened);
      assertFalse(page.getPref(PrefName.WEB_ACTUATION_ENABLED).value);

      // Toggling the setting to on expands the info card.
      webActuationToggle.$.control.click();
      await flushTasks();
      assertTrue(page.getPref(PrefName.WEB_ACTUATION_ENABLED).value);
      assertTrue(infoCard.opened);
      await verifyUserAction('Glic.Settings.WebActuation.Enabled');

      // Toggling the setting off collapses the info card.
      webActuationToggle.$.control.click();
      await flushTasks();
      assertFalse(page.getPref(PrefName.WEB_ACTUATION_ENABLED).value);
      assertFalse(infoCard.opened);
      await verifyUserAction('Glic.Settings.WebActuation.Disabled');

      // Toggling the setting to on while the info card is open leaves it open.
      webActuationToggle.click();
      await flushTasks();
      assertTrue(infoCard.opened);
      webActuationToggle.$.control.click();
      await flushTasks();
      assertTrue(page.getPref(PrefName.WEB_ACTUATION_ENABLED).value);
      assertTrue(infoCard.opened);

      // Toggling the setting to off while the info card is closed leaves it
      // closed.
      webActuationToggle.click();
      await flushTasks();
      assertFalse(infoCard.opened);
      webActuationToggle.$.control.click();
      await flushTasks();
      assertFalse(page.getPref(PrefName.WEB_ACTUATION_ENABLED).value);
      assertFalse(infoCard.opened);
    });
  });


  suite('WebActuationToggleVisibleForAllowedTier', () => {
    test('assert toggle is visible', () => {
      const webActuationToggle =
          $<SettingsToggleButtonElement>('webActuationToggle')!;
      assertTrue(isVisible(webActuationToggle));
    });
  });

  suite('WebActuationToggleHiddenForDisallowedTier', () => {
    test('assert toggle is hidden', () => {
      const webActuationToggle =
          $<SettingsToggleButtonElement>('webActuationToggle')!;
      assertFalse(isVisible(webActuationToggle));
    });
  });

  suite('WebActuationEnterprisePolicy', () => {
    function waitOneTick() {
      return new Promise(resolve => setTimeout(resolve, 0));
    }

    async function setWebActuationCapability(canActOnWeb: boolean) {
      webUIListenerCallback(
          'glic-web-actuation-capability-changed', canActOnWeb);
      await flushTasks();
      await waitOneTick();
      await flushTasks();
    }

    test('ToggleDisabledByEnterprisePolicy', async () => {
      page.setPrefValue(PrefName.WEB_ACTUATION_ENABLED, true);
      await flushTasks();

      // Verify initial state (enabled).
      let webActuationToggle =
          $<SettingsToggleButtonElement>('webActuationToggle')!;
      assertTrue(isVisible(webActuationToggle));
      assertFalse(webActuationToggle.disabled);

      // Simulate enterprise DISABLING the feature.
      await setWebActuationCapability(false);

      // Re-query, as dom-if restamped.
      webActuationToggle =
          $<SettingsToggleButtonElement>('webActuationToggle')!;
      assertTrue(isVisible(webActuationToggle));
      assertTrue(webActuationToggle.disabled);
      assertFalse(webActuationToggle.checked);
    });

    test('MenuCollapsesWhenDisabledByPolicy', async () => {
      const webActuationToggle =
          $<SettingsToggleButtonElement>('webActuationToggle')!;
      let infoCard = $<CrCollapseElement>('webActuationInfoCollapse')!;
      page.setPrefValue(PrefName.WEB_ACTUATION_ENABLED, false);
      assertFalse(infoCard.opened);
      webActuationToggle.click();
      await flushTasks();
      assertTrue(infoCard.opened);
      assertFalse(page.getPref(PrefName.WEB_ACTUATION_ENABLED).value);

      // Simulate enterprise DISABLING the feature.
      await setWebActuationCapability(false);

      // Re-query, as dom-if restamped.
      infoCard = $<CrCollapseElement>('webActuationInfoCollapse')!;
      assertTrue(!!infoCard);
      assertFalse(infoCard.opened);
    });

    test('PrefDoesNotExpandMenuWhenDisabledByPolicy', async () => {
      // Start disabled by enterprise.
      await setWebActuationCapability(false);

      const infoCard = $<CrCollapseElement>('webActuationInfoCollapse')!;
      assertTrue(!!infoCard);        // It exists.
      assertFalse(infoCard.opened);  // Starts closed.

      // Try to enable it via pref (e.g. from sync).
      page.setPrefValue(PrefName.WEB_ACTUATION_ENABLED, true);
      await flushTasks();

      // Should still be closed because webActuationEnabledExpanded_
      // remains false in the enterprise disabled state.
      assertFalse(infoCard.opened);
    });

    test('ToggleReEnablesWhenPolicyAllows', async () => {
      // Start disabled.
      await setWebActuationCapability(false);
      let webActuationToggle =
          $<SettingsToggleButtonElement>('webActuationToggle')!;
      assertTrue(webActuationToggle.disabled);

      // Simulate enterprise ENABLING it back.
      await setWebActuationCapability(true);

      webActuationToggle =
          $<SettingsToggleButtonElement>('webActuationToggle')!;
      assertFalse(webActuationToggle.disabled);
    });
  });

  suite('DataProtection_UserStatusCheckEnabled', () => {
    test('DataProtectionStringsShownForEligibleUser', () => {
      page.setPrefValue(
          PrefName.USER_STATUS, {isEnterpriseAccountDataProtected: true});
      const locationToggle =
          $<SettingsToggleButtonElement>('geolocationToggle')!;
      assertEquals(
          page.i18n('glicLocationToggleSublabelDataProtected'),
          locationToggle.subLabel);
      assertEquals('', locationToggle.learnMoreUrl);
      const microphoneToggle =
          $<SettingsToggleButtonElement>('microphoneToggle')!;
      assertEquals(
          page.i18n('glicMicrophoneToggleSublabelDataProtected'),
          microphoneToggle.subLabel);
      assertEquals(undefined, microphoneToggle.learnMoreUrl);
      const tabAccessToggle =
          $<SettingsToggleButtonElement>('tabAccessToggle')!;
      assertEquals(
          page.i18n('glicTabAccessToggleSublabelDataProtected'),
          tabAccessToggle.subLabel);
      const learnMoreLabel =
          $<HTMLAnchorElement>('shortcutTabAccessConsider1LearnMoreLabel')!;
      assertEquals('https://example.com/data-protection', learnMoreLabel.href);
    });

    test('DataProtectionStringsNotShownForIneligibleUser', () => {
      page.setPrefValue(
          PrefName.USER_STATUS, {isEnterpriseAccountDataProtected: false});
      const locationToggle =
          $<SettingsToggleButtonElement>('geolocationToggle')!;
      assertEquals(
          page.i18n('glicLocationToggleSublabel'), locationToggle.subLabel);
      assertEquals(
          page.i18n('glicLocationToggleLearnMoreUrl'),
          locationToggle.learnMoreUrl);
      const microphoneToggle =
          $<SettingsToggleButtonElement>('microphoneToggle')!;
      assertEquals(
          page.i18n('glicMicrophoneToggleSublabel'), microphoneToggle.subLabel);
      assertEquals(undefined, microphoneToggle.learnMoreUrl);
      const tabAccessToggle =
          $<SettingsToggleButtonElement>('tabAccessToggle')!;
      assertEquals(
          page.i18n('glicTabAccessToggleSublabel'), tabAccessToggle.subLabel);
      const learnMoreLabel =
          $<HTMLAnchorElement>('shortcutTabAccessConsider1LearnMoreLabel')!;
      assertEquals('https://example.com/tab-access', learnMoreLabel.href);
    });
  });

  suite('DataProtection_UserStatusCheckDisabled', () => {
    test('DataProtectionStringsNotShown', () => {
      page.setPrefValue(
          PrefName.USER_STATUS, {isEnterpriseAccountDataProtected: true});
      const locationToggle =
          $<SettingsToggleButtonElement>('geolocationToggle')!;
      assertEquals(
          page.i18n('glicLocationToggleSublabel'), locationToggle.subLabel);
      assertEquals(
          page.i18n('glicLocationToggleLearnMoreUrl'),
          locationToggle.learnMoreUrl);
      const microphoneToggle =
          $<SettingsToggleButtonElement>('microphoneToggle')!;
      assertEquals(
          page.i18n('glicMicrophoneToggleSublabel'), microphoneToggle.subLabel);
      assertEquals(undefined, microphoneToggle.learnMoreUrl);
      const tabAccessToggle =
          $<SettingsToggleButtonElement>('tabAccessToggle')!;
      assertEquals(
          page.i18n('glicTabAccessToggleSublabel'), tabAccessToggle.subLabel);
      const learnMoreLabel =
          $<HTMLAnchorElement>('shortcutTabAccessConsider1LearnMoreLabel')!;
      assertEquals('https://example.com/tab-access', learnMoreLabel.href);
    });
  });
});
