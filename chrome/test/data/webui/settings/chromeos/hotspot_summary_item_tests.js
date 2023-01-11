// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {setHotspotConfigForTesting} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.js';
import {FakeHotspotConfig} from 'chrome://resources/ash/common/hotspot/fake_hotspot_config.js';
import {CrosHotspotConfigInterface, CrosHotspotConfigObserverInterface, HotspotAllowStatus, HotspotConfig, HotspotControlResult, HotspotInfo, HotspotState, WiFiSecurityMode} from 'chrome://resources/mojo/chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('HotspotSummaryItemTest', function() {
  /** @type {HotspotSummaryItemElement} */
  let hotspotSummaryItem = null;

  /** @type {?CrosHotspotConfigInterface} */
  let hotspotConfig_ = null;

  /**
   * @type {!CrosHotspotConfigObserverInterface}
   */
  let hotspotConfigObserver;

  suiteSetup(function() {
    hotspotConfig_ = new FakeHotspotConfig();
    setHotspotConfigForTesting(hotspotConfig_);
  });

  setup(async function() {
    PolymerTest.clearBody();

    hotspotSummaryItem = document.createElement('hotspot-summary-item');
    document.body.appendChild(hotspotSummaryItem);
    flush();

    hotspotConfigObserver = {
      /** override */
      onHotspotInfoChanged() {
        hotspotConfig_.getHotspotInfo().then(response => {
          hotspotSummaryItem.hotspotInfo = response.hotspotInfo;
        });
      },
    };
    hotspotConfig_.addObserver(hotspotConfigObserver);

    hotspotConfig_.setFakeHotspotInfo({
      state: HotspotState.kDisabled,
      allowStatus: HotspotAllowStatus.kAllowed,
    });
    await flushAsync();
  });

  teardown(function() {
    hotspotSummaryItem.remove();
    hotspotSummaryItem = null;
    Router.getInstance().resetRouteForTesting();
    hotspotConfig_.reset();
  });

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  test(
      'clicking on subpage arrow routes to hotspot subpage when allowed',
      async function() {
        const subpageArrow = hotspotSummaryItem.shadowRoot.querySelector(
            '#hotspotSummaryItemRowArrowIcon');
        assertTrue(!!subpageArrow, 'Subpage arrow should exist');
        subpageArrow.click();
        assertEquals(
            routes.HOTSPOT_DETAIL, Router.getInstance().getCurrentRoute());
      });

  test(
      'clicking on hotspot summary row routes to hotspot subpage when allowed',
      async function() {
        const hotspotSummaryRow = hotspotSummaryItem.shadowRoot.querySelector(
            '#hotspotSummaryItemRow');
        assertTrue(!!hotspotSummaryRow, 'Hotspot summary row should exist');
        hotspotSummaryRow.click();
        assertEquals(
            routes.HOTSPOT_DETAIL, Router.getInstance().getCurrentRoute());
      });

  test('UI state when hotspot is allowed and state is off', async function() {
    const hotspotStateSublabel =
        hotspotSummaryItem.shadowRoot.querySelector('#hotspotStateSublabel');
    const hotspotDisabledSublabelLink =
        hotspotSummaryItem.shadowRoot.querySelector(
            '#hotspotDisabledSublabelLink');
    const enableToggle =
        hotspotSummaryItem.shadowRoot.querySelector('#enableHotspotToggle');
    const hotspotIcon =
        hotspotSummaryItem.shadowRoot.querySelector('#hotspotIcon');
    const subpageArrow = hotspotSummaryItem.shadowRoot.querySelector(
        '#hotspotSummaryItemRowArrowIcon');
    const policyIndicator =
        hotspotSummaryItem.shadowRoot.querySelector('#policyIndicator');

    assertFalse(enableToggle.disabled, 'Toggle should be enabled');
    assertTrue(!!subpageArrow, 'Subpage arrow should exist');
    assertFalse(!!policyIndicator, 'Policy indicator should not exist');
    assertEquals('os-settings:hotspot-disabled', hotspotIcon.icon);
    assertFalse(hotspotStateSublabel.hidden, 'State sublabel should show');
    assertEquals(
        hotspotSummaryItem.i18n('hotspotSummaryStateOff'),
        hotspotStateSublabel.textContent.trim());
    assertTrue(
        hotspotDisabledSublabelLink.hidden,
        'Disabled sublabel link should hide');
  });

  test('UI state when hotspot is allowed and state is on', async function() {
    hotspotConfig_.setFakeHotspotState(HotspotState.kEnabled);
    await flushAsync();

    const hotspotStateSublabel =
        hotspotSummaryItem.shadowRoot.querySelector('#hotspotStateSublabel');
    const hotspotDisabledSublabelLink =
        hotspotSummaryItem.shadowRoot.querySelector(
            '#hotspotDisabledSublabelLink');
    const enableToggle =
        hotspotSummaryItem.shadowRoot.querySelector('#enableHotspotToggle');
    const hotspotIcon =
        hotspotSummaryItem.shadowRoot.querySelector('#hotspotIcon');
    const subpageArrow = hotspotSummaryItem.shadowRoot.querySelector(
        '#hotspotSummaryItemRowArrowIcon');
    const policyIndicator =
        hotspotSummaryItem.shadowRoot.querySelector('#policyIndicator');

    assertFalse(enableToggle.disabled, 'Toggle should be enabled');
    assertTrue(!!subpageArrow, 'Subpage arrow should exist');
    assertFalse(!!policyIndicator, 'Policy indicator should not exist');
    assertEquals('os-settings:hotspot-enabled', hotspotIcon.icon);
    assertFalse(hotspotStateSublabel.hidden, 'State sublabel should show');
    assertEquals(
        hotspotSummaryItem.i18n('hotspotSummaryStateOn'),
        hotspotStateSublabel.textContent.trim());
    assertTrue(
        hotspotDisabledSublabelLink.hidden,
        'Disabled sublabel link should hide');
  });

  test('UI state when disallowed by policy', async function() {
    hotspotConfig_.setFakeHotspotAllowStatus(
        HotspotAllowStatus.kDisallowedByPolicy);
    await flushAsync();

    const hotspotStateSublabel =
        hotspotSummaryItem.shadowRoot.querySelector('#hotspotStateSublabel');
    const hotspotDisabledSublabelLink =
        hotspotSummaryItem.shadowRoot.querySelector(
            '#hotspotDisabledSublabelLink');
    const enableToggle =
        hotspotSummaryItem.shadowRoot.querySelector('#enableHotspotToggle');
    const hotspotIcon =
        hotspotSummaryItem.shadowRoot.querySelector('#hotspotIcon');
    const subpageArrow = hotspotSummaryItem.shadowRoot.querySelector(
        '#hotspotSummaryItemRowArrowIcon');
    const policyIndicator =
        hotspotSummaryItem.shadowRoot.querySelector('#policyIndicator');

    // Toggle should be disabled, subpage arrow should not show.
    assertTrue(enableToggle.disabled, 'Toggle should be disabled');
    assertFalse(!!subpageArrow, 'Subpage arrow should not exist');
    assertTrue(!!policyIndicator, 'Policy indicator should exist');
    assertEquals('os-settings:hotspot-disabled', hotspotIcon.icon);
    assertFalse(hotspotStateSublabel.hidden, 'State sublabel should show');
    assertEquals(
        hotspotSummaryItem.i18n('hotspotSummaryStateOff'),
        hotspotStateSublabel.textContent.trim());
    assertTrue(
        hotspotDisabledSublabelLink.hidden,
        'Disabled sublabel link should hide');

    // Verify toggle is able to turn on/off by CrosHotspotConfig even when it is
    // disabled by policy.
    hotspotConfig_.setFakeHotspotState(HotspotState.kEnabled);
    await flushAsync();
    assertEquals(
        hotspotSummaryItem.i18n('hotspotSummaryStateOn'),
        hotspotStateSublabel.textContent.trim());
    assertEquals('os-settings:hotspot-enabled', hotspotIcon.icon);

    hotspotConfig_.setFakeHotspotState(HotspotState.kDisabled);
    await flushAsync();
    assertEquals(
        hotspotSummaryItem.i18n('hotspotSummaryStateOff'),
        hotspotStateSublabel.textContent.trim());
    assertEquals('os-settings:hotspot-disabled', hotspotIcon.icon);
  });

  test(
      'UI state when mobile data plan doesn\'t support hotspot',
      async function() {
        hotspotConfig_.setFakeHotspotAllowStatus(
            HotspotAllowStatus.kDisallowedReadinessCheckFail);
        await flushAsync();

        const hotspotStateSublabel =
            hotspotSummaryItem.shadowRoot.querySelector(
                '#hotspotStateSublabel');
        const hotspotDisabledSublabelLink =
            hotspotSummaryItem.shadowRoot.querySelector(
                '#hotspotDisabledSublabelLink');
        const enableToggle =
            hotspotSummaryItem.shadowRoot.querySelector('#enableHotspotToggle');
        const hotspotIcon =
            hotspotSummaryItem.shadowRoot.querySelector('#hotspotIcon');
        const subpageArrow = hotspotSummaryItem.shadowRoot.querySelector(
            '#hotspotSummaryItemRowArrowIcon');
        const policyIndicator =
            hotspotSummaryItem.shadowRoot.querySelector('#policyIndicator');

        // Toggle should be disabled, subpage arrow should not show.
        assertTrue(enableToggle.disabled, 'Toggle should be disabled');
        assertFalse(!!subpageArrow, 'Subpage arrow should not exist');
        assertFalse(!!policyIndicator, 'Policy indicator should not exist');
        assertEquals('os-settings:hotspot-disabled', hotspotIcon.icon);
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

  test('UI state when no mobile data connection', async function() {
    hotspotConfig_.setFakeHotspotAllowStatus(
        HotspotAllowStatus.kDisallowedNoMobileData);
    await flushAsync();

    const hotspotStateSublabel =
        hotspotSummaryItem.shadowRoot.querySelector('#hotspotStateSublabel');
    const hotspotDisabledSublabelLink =
        hotspotSummaryItem.shadowRoot.querySelector(
            '#hotspotDisabledSublabelLink');
    const enableToggle =
        hotspotSummaryItem.shadowRoot.querySelector('#enableHotspotToggle');
    const hotspotIcon =
        hotspotSummaryItem.shadowRoot.querySelector('#hotspotIcon');
    const subpageArrow = hotspotSummaryItem.shadowRoot.querySelector(
        '#hotspotSummaryItemRowArrowIcon');
    const policyIndicator =
        hotspotSummaryItem.shadowRoot.querySelector('#policyIndicator');

    // Toggle should be disabled, subpage arrow should not show.
    assertTrue(enableToggle.disabled, 'Toggle should be disabled');
    assertFalse(!!subpageArrow, 'Subpage arrow should not exist');
    assertFalse(!!policyIndicator, 'Policy indicator should not exist');
    assertEquals('os-settings:hotspot-disabled', hotspotIcon.icon);
    assertTrue(hotspotStateSublabel.hidden, 'State sublabel should hide');
    assertFalse(
        hotspotDisabledSublabelLink.hidden,
        'Disabled sublabel link should show');
    assertEquals(
        hotspotSummaryItem.i18nAdvanced('hotspotNoMobileDataSublabelWithLink')
            .toString(),
        hotspotDisabledSublabelLink.localizedString);
  });

  test('Toggle button state', async function() {
    const enableHotspotToggle =
        hotspotSummaryItem.shadowRoot.querySelector('#enableHotspotToggle');
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