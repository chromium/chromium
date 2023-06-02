// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {CrToggleElement, HotspotSummaryItemElement, LocalizedLinkElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {setHotspotConfigForTesting} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.js';
import {CrosHotspotConfigInterface, CrosHotspotConfigObserverInterface, CrosHotspotConfigObserverRemote, HotspotAllowStatus, HotspotControlResult, HotspotState, WiFiSecurityMode} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.mojom-webui.js';
import {FakeHotspotConfig} from 'chrome://resources/ash/common/hotspot/fake_hotspot_config.js';
import {CrPolicyIndicatorElement} from 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('<hotspot-summary-item>', () => {
  let hotspotSummaryItem: HotspotSummaryItemElement;
  let hotspotConfig_: CrosHotspotConfigInterface&FakeHotspotConfig;
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
    hotspotConfig_ = new FakeHotspotConfig();
    setHotspotConfigForTesting(hotspotConfig_);
  });

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(async () => {
    hotspotSummaryItem = document.createElement('hotspot-summary-item');
    document.body.appendChild(hotspotSummaryItem);
    flush();

    hotspotConfigObserver = {
      async onHotspotInfoChanged() {
        const response = await hotspotConfig_.getHotspotInfo();
        hotspotSummaryItem.hotspotInfo = response.hotspotInfo;
      },
    };
    hotspotConfig_.addObserver(
        hotspotConfigObserver as CrosHotspotConfigObserverRemote);

    hotspotConfig_.setFakeHotspotInfo({
      state: HotspotState.kDisabled,
      allowStatus: HotspotAllowStatus.kAllowed,
      clientCount: 0,
      allowedWifiSecurityModes: [
        WiFiSecurityMode.MIN_VALUE,
      ],
    });
    await flushAsync();
  });

  teardown(() => {
    hotspotSummaryItem.remove();
    Router.getInstance().resetRouteForTesting();
    hotspotConfig_.reset();
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
    hotspotConfig_.setFakeHotspotState(HotspotState.kEnabled);
    await flushAsync();

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

  test('UI state when disallowed by policy', async () => {
    hotspotConfig_.setFakeHotspotAllowStatus(
        HotspotAllowStatus.kDisallowedByPolicy);
    await flushAsync();

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
    hotspotConfig_.setFakeHotspotState(HotspotState.kEnabled);
    await flushAsync();
    assertEquals(
        hotspotSummaryItem.i18n('hotspotSummaryStateOn'),
        hotspotStateSublabel.textContent!.trim());
    icon = hotspotIcon.shadowRoot!.querySelector<HTMLElement>('#icon');
    assertTrue(!!icon);
    assertTrue(icon.classList.contains('hotspot-on'));

    hotspotConfig_.setFakeHotspotState(HotspotState.kDisabled);
    await flushAsync();
    assertEquals(
        hotspotSummaryItem.i18n('hotspotSummaryStateOff'),
        hotspotStateSublabel.textContent!.trim());
    icon = hotspotIcon.shadowRoot!.querySelector<HTMLElement>('#icon');
    assertTrue(!!icon);
    assertTrue(icon.classList.contains('hotspot-off'));
  });

  test('UI state when mobile data plan doesn\'t support hotspot', async () => {
    hotspotConfig_.setFakeHotspotAllowStatus(
        HotspotAllowStatus.kDisallowedReadinessCheckFail);
    await flushAsync();

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
    hotspotConfig_.setFakeHotspotAllowStatus(
        HotspotAllowStatus.kDisallowedNoMobileData);
    await flushAsync();

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

  test('Toggle button state', async () => {
    const enableHotspotToggle = queryEnableHotspotToggle();
    assertTrue(!!enableHotspotToggle, 'Hotspot enable toggl should exist');
    assertFalse(enableHotspotToggle.checked);

    // Simulate clicking toggle to turn on hotspot and fail.
    hotspotConfig_.setFakeEnableHotspotResult(
        HotspotControlResult.kNetworkSetupFailure);
    enableHotspotToggle.click();
    await flushAsync();
    // Toggle should be off.
    assertFalse(enableHotspotToggle.checked);
    assertFalse(enableHotspotToggle.disabled);

    // Simulate clicking toggle to turn on hotspot and succeed.
    let a11yMessagesEventPromise =
        eventToPromise('cr-a11y-announcer-messages-sent', document.body);
    hotspotConfig_.setFakeEnableHotspotResult(HotspotControlResult.kSuccess);
    enableHotspotToggle.click();
    await flushAsync();
    // Toggle should be on this time.
    assertTrue(enableHotspotToggle.checked);
    assertFalse(enableHotspotToggle.disabled);
    let a11yMessagesEvent = await a11yMessagesEventPromise;
    assertTrue(a11yMessagesEvent.detail.messages.includes(
        hotspotSummaryItem.i18n('hotspotEnabledA11yLabel')));

    // Simulate clicking on toggle to turn off hotspot and succeed.
    a11yMessagesEventPromise =
        eventToPromise('cr-a11y-announcer-messages-sent', document.body);
    hotspotConfig_.setFakeDisableHotspotResult(HotspotControlResult.kSuccess);
    enableHotspotToggle.click();
    await flushAsync();
    // Toggle should be off
    assertFalse(enableHotspotToggle.checked);
    assertFalse(enableHotspotToggle.disabled);
    a11yMessagesEvent = await a11yMessagesEventPromise;
    assertTrue(a11yMessagesEvent.detail.messages.includes(
        hotspotSummaryItem.i18n('hotspotDisabledA11yLabel')));

    // Simulate state becoming kEnabling.
    hotspotConfig_.setFakeHotspotState(HotspotState.kEnabling);
    await flushAsync();
    // Toggle should be disabled.
    assertTrue(enableHotspotToggle.disabled);
    hotspotConfig_.setFakeHotspotState(HotspotState.kDisabled);

    // Simulate AllowStatus becoming kDisallowedByPolicy.
    hotspotConfig_.setFakeHotspotAllowStatus(
        HotspotAllowStatus.kDisallowedByPolicy);
    await flushAsync();
    // Toggle should be disabled.
    assertTrue(enableHotspotToggle.disabled);
  });
});
