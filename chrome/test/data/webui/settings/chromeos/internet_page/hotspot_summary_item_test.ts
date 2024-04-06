// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {CrPolicyIndicatorElement, CrToggleElement, HotspotSummaryItemElement, LocalizedLinkElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {setHotspotConfigForTesting} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.js';
import {CrosHotspotConfigInterface, CrosHotspotConfigObserverInterface, CrosHotspotConfigObserverRemote, HotspotAllowStatus, HotspotControlResult, HotspotState, WiFiSecurityMode} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.mojom-webui.js';
import {FakeHotspotConfig} from 'chrome://resources/ash/common/hotspot/fake_hotspot_config.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('<hotspot-summary-item>', () => {
  let hotspotSummaryItem: HotspotSummaryItemElement;
  let hotspotConfig: CrosHotspotConfigInterface&FakeHotspotConfig;
  let hotspotConfigObserver: CrosHotspotConfigObserverInterface;

  function queryHotspotStateSublabel(): HTMLElement|null {
    return hotspotSummaryItem.shadowRoot!.querySelector<HTMLElement>(
        '#hotspotStateSublabel');
  }

  function queryHotspotDisabledSublabelLink(): LocalizedLinkElement|null {
    return hotspotSummaryItem.shadowRoot!.querySelector<LocalizedLinkElement>(
        '#hotspotDisabledSublabelLink');
  }

  function queryEnableHotspotToggle(): CrToggleElement|null {
    return hotspotSummaryItem.shadowRoot!.querySelector<CrToggleElement>(
        '#enableHotspotToggle');
  }

  function queryHotspotIcon(): HTMLElement|null {
    return hotspotSummaryItem.shadowRoot!.querySelector<HTMLElement>(
        '#hotspotIcon');
  }

  function queryHotspotSummaryItemRowArrowIcon(): HTMLElement|null {
    return hotspotSummaryItem.shadowRoot!.querySelector(
        '#hotspotSummaryItemRowArrowIcon');
  }

  function queryPolicyIndicator(): CrPolicyIndicatorElement|null {
    return hotspotSummaryItem.shadowRoot!.querySelector('#policyIndicator');
  }

  suiteSetup(() => {
    hotspotConfig = new FakeHotspotConfig();
    setHotspotConfigForTesting(hotspotConfig);
  });

  setup(async () => {
    hotspotSummaryItem = document.createElement('hotspot-summary-item');
    document.body.appendChild(hotspotSummaryItem);
    flush();

    hotspotConfigObserver = {
      async onHotspotInfoChanged() {
        const response = await hotspotConfig.getHotspotInfo();
        hotspotSummaryItem.hotspotInfo = response.hotspotInfo;
      },
    };
    hotspotConfig.addObserver(
        hotspotConfigObserver as CrosHotspotConfigObserverRemote);

    hotspotConfig.setFakeHotspotInfo({
      state: HotspotState.kDisabled,
      allowStatus: HotspotAllowStatus.kAllowed,
      clientCount: 0,
      allowedWifiSecurityModes: [
        WiFiSecurityMode.MIN_VALUE,
      ],
      config: null,
    });
    await flushTasks();
  });

  teardown(() => {
    hotspotSummaryItem.remove();
    Router.getInstance().resetRouteForTesting();
    hotspotConfig.reset();
  });

  test(
      'clicking on subpage arrow routes to hotspot subpage when allowed',
      () => {
        const subpageArrow = queryHotspotSummaryItemRowArrowIcon();
        assertTrue(!!subpageArrow, 'Subpage arrow should exist');
        subpageArrow.click();
        assertEquals(routes.HOTSPOT_DETAIL, Router.getInstance().currentRoute);
      });

  test(
      'clicking on hotspot summary row routes to hotspot subpage when allowed',
      () => {
        const hotspotSummaryRow =
            hotspotSummaryItem.shadowRoot!.querySelector<HTMLElement>(
                '#hotspotSummaryItemRow');
        assertTrue(!!hotspotSummaryRow, 'Hotspot summary row should exist');
        hotspotSummaryRow.click();
        assertEquals(routes.HOTSPOT_DETAIL, Router.getInstance().currentRoute);
      });

  test('UI state when hotspot is allowed and state is off', () => {
    const hotspotStateSublabel = queryHotspotStateSublabel();
    assertTrue(!!hotspotStateSublabel);
    const hotspotDisabledSublabelLink = queryHotspotDisabledSublabelLink();
    assertTrue(!!hotspotDisabledSublabelLink);
    const enableToggle = queryEnableHotspotToggle();
    assertTrue(!!enableToggle);
    const hotspotIcon = queryHotspotIcon();
    assertTrue(!!hotspotIcon);
    const subpageArrow = queryHotspotSummaryItemRowArrowIcon();
    const policyIndicator = queryPolicyIndicator();

    assertFalse(enableToggle.disabled, 'Toggle should be enabled');
    assertTrue(!!subpageArrow, 'Subpage arrow should exist');
    assertNull(policyIndicator, 'Policy indicator should not exist');
    const icon = hotspotIcon.shadowRoot!.querySelector<HTMLElement>('#icon');
    assertTrue(!!icon);
    assertTrue(icon.classList.contains('hotspot-off'));
    assertFalse(hotspotStateSublabel.hidden, 'State sublabel should show');
    assertEquals(
        hotspotSummaryItem.i18n('hotspotSummaryStateOff'),
        hotspotStateSublabel.textContent!.trim());
    assertTrue(
        hotspotDisabledSublabelLink.hidden,
        'Disabled sublabel link should hide');
  });

  test('UI state when hotspot is allowed and state is on', async () => {
    hotspotConfig.setFakeHotspotState(HotspotState.kEnabled);
    await flushTasks();

    const hotspotStateSublabel = queryHotspotStateSublabel();
    assertTrue(!!hotspotStateSublabel);
    const hotspotDisabledSublabelLink = queryHotspotDisabledSublabelLink();
    assertTrue(!!hotspotDisabledSublabelLink);
    const enableToggle = queryEnableHotspotToggle();
    assertTrue(!!enableToggle);
    const hotspotIcon = queryHotspotIcon();
    assertTrue(!!hotspotIcon);
    const subpageArrow = queryHotspotSummaryItemRowArrowIcon();
    const policyIndicator = queryPolicyIndicator();

    assertFalse(enableToggle.disabled, 'Toggle should be enabled');
    assertTrue(!!subpageArrow, 'Subpage arrow should exist');
    assertNull(policyIndicator, 'Policy indicator should not exist');
    const icon = hotspotIcon.shadowRoot!.querySelector<HTMLElement>('#icon');
    assertTrue(!!icon);
    assertTrue(icon.classList.contains('hotspot-on'));
    assertFalse(hotspotStateSublabel.hidden, 'State sublabel should show');
    assertEquals(
        hotspotSummaryItem.i18n('hotspotSummaryStateOn'),
        hotspotStateSublabel.textContent!.trim());
    assertTrue(
        hotspotDisabledSublabelLink.hidden,
        'Disabled sublabel link should hide');
  });

  test('Hotspot sublabel in various hotspot states', async () => {
    hotspotConfig.setFakeHotspotState(HotspotState.kEnabling);
    await flushTasks();
    const hotspotStateSublabel = queryHotspotStateSublabel();
    assertTrue(!!hotspotStateSublabel);
    assertEquals(
        hotspotSummaryItem.i18n('hotspotSummaryStateTurningOn'),
        hotspotStateSublabel.textContent!.trim());

    hotspotConfig.setFakeHotspotState(HotspotState.kEnabled);
    await flushTasks();
    assertEquals(
        hotspotSummaryItem.i18n('hotspotSummaryStateOn'),
        hotspotStateSublabel.textContent!.trim());

    hotspotConfig.setFakeHotspotState(HotspotState.kDisabling);
    await flushTasks();
    assertEquals(
        hotspotSummaryItem.i18n('hotspotSummaryStateTurningOff'),
        hotspotStateSublabel.textContent!.trim());

    hotspotConfig.setFakeHotspotState(HotspotState.kDisabled);
    await flushTasks();
    assertEquals(
        hotspotSummaryItem.i18n('hotspotSummaryStateOff'),
        hotspotStateSublabel.textContent!.trim());
  });

  test('UI state when disallowed by policy', async () => {
    hotspotConfig.setFakeHotspotAllowStatus(
        HotspotAllowStatus.kDisallowedByPolicy);
    await flushTasks();

    const hotspotStateSublabel = queryHotspotStateSublabel();
    assertTrue(!!hotspotStateSublabel);
    const hotspotDisabledSublabelLink = queryHotspotDisabledSublabelLink();
    assertTrue(!!hotspotDisabledSublabelLink);
    const enableToggle = queryEnableHotspotToggle();
    assertTrue(!!enableToggle);
    const hotspotIcon = queryHotspotIcon();
    assertTrue(!!hotspotIcon);
    const subpageArrow = queryHotspotSummaryItemRowArrowIcon();
    const policyIndicator = queryPolicyIndicator();

    // Toggle should be disabled, subpage arrow should not show.
    assertTrue(enableToggle.disabled, 'Toggle should be disabled');
    assertNull(subpageArrow, 'Subpage arrow should not exist');
    assertTrue(!!policyIndicator, 'Policy indicator should exist');
    let icon = hotspotIcon.shadowRoot!.querySelector<HTMLElement>('#icon');
    assertTrue(!!icon);
    assertTrue(icon.classList.contains('hotspot-off'));
    assertFalse(hotspotStateSublabel.hidden, 'State sublabel should show');
    assertEquals(
        hotspotSummaryItem.i18n('hotspotSummaryStateOff'),
        hotspotStateSublabel.textContent!.trim());
    assertTrue(
        hotspotDisabledSublabelLink.hidden,
        'Disabled sublabel link should hide');

    // Verify toggle is able to turn on/off by CrosHotspotConfig even when it is
    // disabled by policy.
    hotspotConfig.setFakeHotspotState(HotspotState.kEnabled);
    await flushTasks();
    assertEquals(
        hotspotSummaryItem.i18n('hotspotSummaryStateOn'),
        hotspotStateSublabel.textContent!.trim());
    icon = hotspotIcon.shadowRoot!.querySelector<HTMLElement>('#icon');
    assertTrue(!!icon);
    assertTrue(icon.classList.contains('hotspot-on'));

    hotspotConfig.setFakeHotspotState(HotspotState.kDisabled);
    await flushTasks();
    assertEquals(
        hotspotSummaryItem.i18n('hotspotSummaryStateOff'),
        hotspotStateSublabel.textContent!.trim());
    icon = hotspotIcon.shadowRoot!.querySelector<HTMLElement>('#icon');
    assertTrue(!!icon);
    assertTrue(icon.classList.contains('hotspot-off'));
  });

  test('UI state when mobile data plan doesn\'t support hotspot', async () => {
    hotspotConfig.setFakeHotspotAllowStatus(
        HotspotAllowStatus.kDisallowedReadinessCheckFail);
    await flushTasks();

    const hotspotStateSublabel = queryHotspotStateSublabel();
    assertTrue(!!hotspotStateSublabel);
    const hotspotDisabledSublabelLink = queryHotspotDisabledSublabelLink();
    assertTrue(!!hotspotDisabledSublabelLink);
    const enableToggle = queryEnableHotspotToggle();
    assertTrue(!!enableToggle);
    const hotspotIcon = queryHotspotIcon();
    assertTrue(!!hotspotIcon);
    const subpageArrow = queryHotspotSummaryItemRowArrowIcon();
    const policyIndicator = queryPolicyIndicator();

    // Toggle should be disabled, subpage arrow should not show.
    assertTrue(enableToggle.disabled, 'Toggle should be disabled');
    assertNull(subpageArrow, 'Subpage arrow should not exist');
    assertNull(policyIndicator, 'Policy indicator should not exist');
    const icon = hotspotIcon.shadowRoot!.querySelector<HTMLElement>('#icon');
    assertTrue(!!icon);
    assertTrue(icon.classList.contains('hotspot-off'));
    assertTrue(hotspotStateSublabel.hidden, 'State sublabel should hide');
    assertFalse(
        hotspotDisabledSublabelLink.hidden,
        'Disabled sublabel link should show');
    assertEquals(
        hotspotSummaryItem
            .i18nAdvanced('hotspotMobileDataNotSupportedSublabelWithLink')
            .toString(),
        hotspotDisabledSublabelLink.localizedString);
  });

  test('UI state when no mobile data connection', async () => {
    hotspotConfig.setFakeHotspotAllowStatus(
        HotspotAllowStatus.kDisallowedNoMobileData);
    await flushTasks();

    const hotspotStateSublabel = queryHotspotStateSublabel();
    assertTrue(!!hotspotStateSublabel);
    const hotspotDisabledSublabelLink = queryHotspotDisabledSublabelLink();
    assertTrue(!!hotspotDisabledSublabelLink);
    const enableToggle = queryEnableHotspotToggle();
    assertTrue(!!enableToggle);
    const hotspotIcon = queryHotspotIcon();
    assertTrue(!!hotspotIcon);
    const subpageArrow = queryHotspotSummaryItemRowArrowIcon();
    const policyIndicator = queryPolicyIndicator();

    // Toggle should be disabled, subpage arrow should not show.
    assertTrue(enableToggle.disabled, 'Toggle should be disabled');
    assertNull(subpageArrow, 'Subpage arrow should not exist');
    assertNull(policyIndicator, 'Policy indicator should not exist');
    const icon = hotspotIcon.shadowRoot!.querySelector<HTMLElement>('#icon');
    assertTrue(!!icon);
    assertTrue(icon.classList.contains('hotspot-off'));
    assertTrue(hotspotStateSublabel.hidden, 'State sublabel should hide');
    assertFalse(
        hotspotDisabledSublabelLink.hidden,
        'Disabled sublabel link should show');
    assertEquals(
        hotspotSummaryItem.i18nAdvanced('hotspotNoMobileDataSublabelWithLink')
            .toString(),
        hotspotDisabledSublabelLink.localizedString);
  });

  test('UI state when no mobile data connection and enabling', async () => {
    hotspotConfig.setFakeHotspotAllowStatus(
        HotspotAllowStatus.kDisallowedNoMobileData);
    hotspotConfig.setFakeHotspotState(HotspotState.kEnabling);
    await flushTasks();

    const subpageArrow = queryHotspotSummaryItemRowArrowIcon();
    const enableToggle = queryEnableHotspotToggle();
    assertTrue(!!enableToggle);
    // Toggle should be enabled, subpage arrow should show.
    assertFalse(enableToggle.disabled, 'Toggle should not be disabled');
    assertTrue(!!subpageArrow, 'Subpage arrow should exist');

    const hotspotStateSublabel = queryHotspotStateSublabel();
    assertTrue(!!hotspotStateSublabel);
    assertEquals(
        hotspotSummaryItem.i18n('hotspotSummaryStateTurningOn'),
        hotspotStateSublabel.textContent!.trim());
  });

  test('Toggle button state', async () => {
    const enableHotspotToggle = queryEnableHotspotToggle();
    assertTrue(!!enableHotspotToggle, 'Hotspot enable toggl should exist');
    assertFalse(enableHotspotToggle.checked);

    // Simulate clicking toggle to turn on hotspot and fail.
    hotspotConfig.setFakeEnableHotspotResult(
        HotspotControlResult.kNetworkSetupFailure);
    enableHotspotToggle.click();
    await flushTasks();
    // Toggle should be off.
    assertFalse(enableHotspotToggle.checked);
    assertFalse(enableHotspotToggle.disabled);

    // Simulate clicking toggle to turn on hotspot and succeed.
    let a11yMessagesEventPromise =
        eventToPromise('cr-a11y-announcer-messages-sent', document.body);
    hotspotConfig.setFakeEnableHotspotResult(HotspotControlResult.kSuccess);
    enableHotspotToggle.click();
    await flushTasks();
    // Toggle should be on this time.
    assertTrue(enableHotspotToggle.checked);
    assertFalse(enableHotspotToggle.disabled);
    let a11yMessagesEvent = await a11yMessagesEventPromise;
    assertTrue(a11yMessagesEvent.detail.messages.includes(
        hotspotSummaryItem.i18n('hotspotEnabledA11yLabel')));

    // Simulate clicking on toggle to turn off hotspot and succeed.
    a11yMessagesEventPromise =
        eventToPromise('cr-a11y-announcer-messages-sent', document.body);
    hotspotConfig.setFakeDisableHotspotResult(HotspotControlResult.kSuccess);
    enableHotspotToggle.click();
    await flushTasks();
    // Toggle should be off
    assertFalse(enableHotspotToggle.checked);
    assertFalse(enableHotspotToggle.disabled);
    a11yMessagesEvent = await a11yMessagesEventPromise;
    assertTrue(a11yMessagesEvent.detail.messages.includes(
        hotspotSummaryItem.i18n('hotspotDisabledA11yLabel')));

    // Simulate state becoming kEnabling.
    hotspotConfig.setFakeHotspotState(HotspotState.kEnabling);
    await flushTasks();
    // Toggle should be enabled to support abort operation.
    assertFalse(enableHotspotToggle.disabled);
    hotspotConfig.setFakeHotspotState(HotspotState.kDisabled);

    // Simulate AllowStatus becoming kDisallowedByPolicy.
    hotspotConfig.setFakeHotspotAllowStatus(
        HotspotAllowStatus.kDisallowedByPolicy);
    await flushTasks();
    // Toggle should be disabled.
    assertTrue(enableHotspotToggle.disabled);
  });
});
