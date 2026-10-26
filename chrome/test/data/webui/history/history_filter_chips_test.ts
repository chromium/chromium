// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import {BrowserProxyImpl} from 'chrome://history/history.js';
import type {HistoryFilterChipsElement} from 'chrome://history/history.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestHistoryBrowserProxy} from './test_browser_proxy.js';

suite('HistoryFilterChipsTest', function() {
  let element: HistoryFilterChipsElement;
  let browserService: TestHistoryBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes?.emptyHTML || '';
    loadTimeData.overrideValues({isCriticalActionsEnabled: false});

    browserService = new TestHistoryBrowserProxy();
    BrowserProxyImpl.setInstance(browserService);

    element = document.createElement('history-filter-chips');
    document.body.appendChild(element);

    await microtasksFinished();
    browserService.resetResolver('recordAction');
  });

  suite('ActorFilter', function() {
    setup(() => {
      loadTimeData.overrideValues({
        isBrowsingHistoryFilterByActorEnabled: true,
      });
    });

    test('VisibilityAndInitialState', async () => {
      // If the parent does not pass `showActorFilter`, do not show the filter
      // by default.
      assertFalse(!!element.shadowRoot.querySelector('#userVisitsChip'));
      assertFalse(!!element.shadowRoot.querySelector('#actorVisitsChip'));
      assertFalse(!!element.shadowRoot.querySelector('#actorFilterChip'));

      element.showActorFilter = true;
      await microtasksFinished();
      const actorFilterChip =
          element.shadowRoot.querySelector<HTMLSelectElement>(
              '#actorFilterChip');
      assertTrue(!!actorFilterChip);
      assertFalse(!!element.shadowRoot.querySelector('#userVisitsChip'));
      assertFalse(!!element.shadowRoot.querySelector('#actorVisitsChip'));
      assertEquals(3, actorFilterChip.options.length);
      assertEquals(
          loadTimeData.getString('sourceFilterAllActors'),
          actorFilterChip.options[0]!.textContent.trim());
      assertTrue(actorFilterChip.options[0]!.selected);
      assertEquals(
          loadTimeData.getString('sourceFilterChipUser'),
          actorFilterChip.options[1]!.textContent.trim());
      assertFalse(actorFilterChip.options[1]!.selected);
      assertEquals(
          loadTimeData.getString('sourceFilterChipActor'),
          actorFilterChip.options[2]!.textContent.trim());
      assertFalse(actorFilterChip.options[2]!.selected);

      element.showActorFilter = false;
      await microtasksFinished();
      assertFalse(!!element.shadowRoot.querySelector('#userVisitsChip'));
      assertFalse(!!element.shadowRoot.querySelector('#actorVisitsChip'));
      assertFalse(!!element.shadowRoot.querySelector('#actorFilterChip'));
    });

    test('SelectActorOptions', async () => {
      element.showActorFilter = true;
      await microtasksFinished();
      const actorFilterChip =
          element.shadowRoot.querySelector<HTMLSelectElement>(
              '#actorFilterChip');
      assertTrue(!!actorFilterChip);

      const icon = element.shadowRoot.querySelector<HTMLElement>(
          '.filter-chips cr-icon');
      assertTrue(!!icon);
      assertFalse(isVisible(icon));

      let eventDetail: {userVisits: boolean, actorVisits: boolean}|undefined;
      element.addEventListener('filter-changed', ((e: CustomEvent) => {
                                                   eventDetail = e.detail;
                                                 }) as EventListener);

      // Select 'User' visits only.
      actorFilterChip.value = 'user';
      actorFilterChip.dispatchEvent(new Event('change'));
      const action1 = await browserService.whenCalled('recordAction');
      assertEquals('HistoryPage_ShowUserOnlyEnabled', action1);
      await microtasksFinished();
      browserService.resetResolver('recordAction');

      assertTrue(!!eventDetail);
      assertTrue(eventDetail.userVisits);
      assertFalse(eventDetail.actorVisits);
      assertFalse(actorFilterChip.options[0]!.selected);
      assertTrue(actorFilterChip.options[1]!.selected);
      assertFalse(actorFilterChip.options[2]!.selected);
      assertTrue(isVisible(icon));
      eventDetail =
          undefined as {userVisits: boolean, actorVisits: boolean} | undefined;

      // Select 'Actor' visits only.
      actorFilterChip.value = 'actor';
      actorFilterChip.dispatchEvent(new Event('change'));
      const action2 = await browserService.whenCalled('recordAction');
      assertEquals('HistoryPage_ShowActorOnlyEnabled', action2);
      await microtasksFinished();
      browserService.resetResolver('recordAction');

      assertTrue(!!eventDetail);
      assertFalse(eventDetail.userVisits);
      assertTrue(eventDetail.actorVisits);
      assertFalse(actorFilterChip.options[0]!.selected);
      assertFalse(actorFilterChip.options[1]!.selected);
      assertTrue(actorFilterChip.options[2]!.selected);
      assertTrue(isVisible(icon));
      eventDetail =
          undefined as {userVisits: boolean, actorVisits: boolean} | undefined;

      // Select 'All sources' (both user and actor visits).
      actorFilterChip.value = 'all';
      actorFilterChip.dispatchEvent(new Event('change'));
      const action3 = await browserService.whenCalled('recordAction');
      assertEquals('HistoryPage_ShowAllEnabled', action3);
      await microtasksFinished();
      browserService.resetResolver('recordAction');

      assertTrue(!!eventDetail);
      assertTrue(eventDetail.userVisits);
      assertTrue(eventDetail.actorVisits);
      assertTrue(actorFilterChip.options[0]!.selected);
      assertFalse(actorFilterChip.options[1]!.selected);
      assertFalse(actorFilterChip.options[2]!.selected);
      assertFalse(isVisible(icon));
    });
  });

  suite('ActorFilterAsChips', function() {
    setup(() => {
      loadTimeData.overrideValues({
        isBrowsingHistoryFilterByActorEnabled: false,
      });
    });

    test('VisibilityAndInitialState', async () => {
      // If the parent does not pass `showActorFilter`, do not show the filter
      // by default.
      assertFalse(!!element.shadowRoot.querySelector('#userVisitsChip'));
      assertFalse(!!element.shadowRoot.querySelector('#actorVisitsChip'));
      assertFalse(!!element.shadowRoot.querySelector('#actorFilterChip'));

      element.showActorFilter = true;
      await microtasksFinished();
      const userChip =
          element.shadowRoot.querySelector<HTMLElement>('#userVisitsChip');
      const actorChip =
          element.shadowRoot.querySelector<HTMLElement>('#actorVisitsChip');
      assertTrue(!!userChip);
      assertTrue(!!actorChip);
      assertFalse(!!element.shadowRoot.querySelector('#actorFilterChip'));
      assertFalse(userChip.hasAttribute('selected'));
      assertFalse(actorChip.hasAttribute('selected'));

      element.showActorFilter = false;
      await microtasksFinished();
      assertFalse(!!element.shadowRoot.querySelector('#userVisitsChip'));
      assertFalse(!!element.shadowRoot.querySelector('#actorVisitsChip'));
      assertFalse(!!element.shadowRoot.querySelector('#actorFilterChip'));
    });

    test('ToggleUserChip', async () => {
      element.showActorFilter = true;
      await microtasksFinished();
      const userChip =
          element.shadowRoot.querySelector<HTMLElement>('#userVisitsChip');
      assertTrue(!!userChip);
      const userIcon = userChip.querySelector('cr-icon');
      assertTrue(!!userIcon);

      let eventDetail: {userVisits: boolean, actorVisits: boolean}|undefined;
      element.addEventListener('filter-changed', ((e: CustomEvent) => {
                                                   eventDetail = e.detail;
                                                 }) as EventListener);

      // Show user visits only.
      userChip.click();
      await microtasksFinished();
      const action1 = await browserService.whenCalled('recordAction');
      assertEquals('HistoryPage_ShowUserOnlyEnabled', action1);
      browserService.resetResolver('recordAction');

      assertTrue(!!eventDetail);
      assertTrue(eventDetail.userVisits);
      assertFalse(eventDetail.actorVisits);
      assertTrue(userChip.hasAttribute('selected'));
      assertEquals('cr:check', userIcon.icon);

      // Show all visits by a second click on the user chip.
      userChip.click();
      await microtasksFinished();
      const action2 = await browserService.whenCalled('recordAction');
      assertEquals('HistoryPage_ShowAllEnabled', action2);

      assertTrue(!!eventDetail);
      assertTrue(eventDetail.userVisits);
      assertTrue(eventDetail.actorVisits);
      assertFalse(userChip.hasAttribute('selected'));
      assertEquals('cr:person-filled', userIcon.icon);
    });

    test('ToggleActorChip', async () => {
      element.showActorFilter = true;
      await microtasksFinished();
      const actorChip =
          element.shadowRoot.querySelector<HTMLElement>('#actorVisitsChip');
      assertTrue(!!actorChip);
      const actorIcon = actorChip.querySelector('cr-icon');
      assertTrue(!!actorIcon);

      let eventDetail: {userVisits: boolean, actorVisits: boolean}|undefined;
      element.addEventListener('filter-changed', ((e: CustomEvent) => {
                                                   eventDetail = e.detail;
                                                 }) as EventListener);

      // Show actor visits only.
      actorChip.click();
      const action1 = await browserService.whenCalled('recordAction');
      assertEquals('HistoryPage_ShowActorOnlyEnabled', action1);
      await microtasksFinished();
      browserService.resetResolver('recordAction');

      assertTrue(!!eventDetail);
      assertFalse(eventDetail.userVisits);
      assertTrue(eventDetail.actorVisits);
      assertTrue(actorChip.hasAttribute('selected'));
      assertEquals('cr:check', actorIcon.icon);

      // Show all visits by a second click on the actor chip.
      actorChip.click();
      const action2 = await browserService.whenCalled('recordAction');
      assertEquals('HistoryPage_ShowAllEnabled', action2);
      await microtasksFinished();

      assertTrue(!!eventDetail);
      assertTrue(eventDetail.userVisits);
      assertTrue(eventDetail.actorVisits);
      assertFalse(actorChip.hasAttribute('selected'));
      // <if expr="_google_chrome">
      assertEquals('history-internal:screensaver-auto', actorIcon.icon);
      // </if>
      // <if expr="not _google_chrome">
      assertEquals('', actorIcon.icon);
      // </if>
    });

    test('ToggleBothChips', async () => {
      element.showActorFilter = true;
      await microtasksFinished();
      const userChip =
          element.shadowRoot.querySelector<HTMLElement>('#userVisitsChip');
      const actorChip =
          element.shadowRoot.querySelector<HTMLElement>('#actorVisitsChip');
      assertTrue(!!userChip && !!actorChip);

      let eventDetail: {userVisits: boolean, actorVisits: boolean}|undefined;
      element.addEventListener('filter-changed', ((e: CustomEvent) => {
                                                   eventDetail = e.detail;
                                                 }) as EventListener);

      // Start at 'User Only' by clicking the user chip.
      userChip.click();
      const action1 = await browserService.whenCalled('recordAction');
      assertEquals('HistoryPage_ShowUserOnlyEnabled', action1);
      await microtasksFinished();
      browserService.resetResolver('recordAction');

      assertTrue(!!eventDetail);
      assertTrue(eventDetail.userVisits);
      assertFalse(eventDetail.actorVisits);
      assertTrue(userChip.hasAttribute('selected'));
      assertFalse(actorChip.hasAttribute('selected'));

      // Clicking Actor Chip while 'User Only' is active should
      // switch directly to 'Actor Only'.
      actorChip.click();
      const action2 = await browserService.whenCalled('recordAction');
      assertEquals('HistoryPage_ShowActorOnlyEnabled', action2);
      await microtasksFinished();
      browserService.resetResolver('recordAction');

      assertTrue(!!eventDetail);
      assertFalse(eventDetail.userVisits);
      assertTrue(eventDetail.actorVisits);
      assertFalse(userChip.hasAttribute('selected'));
      assertTrue(actorChip.hasAttribute('selected'));

      // Clicking User Chip while 'Actor Only' is active should
      // switch back to 'User Only'.
      userChip.click();
      const action3 = await browserService.whenCalled('recordAction');
      assertEquals('HistoryPage_ShowUserOnlyEnabled', action3);
      await microtasksFinished();
      browserService.resetResolver('recordAction');

      assertTrue(!!eventDetail);
      assertTrue(eventDetail.userVisits);
      assertFalse(eventDetail.actorVisits);
      assertTrue(userChip.hasAttribute('selected'));
      assertFalse(actorChip.hasAttribute('selected'));

      // Clicking User Chip again while 'User Only' is active
      // should return to 'Show All'.
      userChip.click();
      const action4 = await browserService.whenCalled('recordAction');
      assertEquals('HistoryPage_ShowAllEnabled', action4);
      await microtasksFinished();

      assertTrue(!!eventDetail);
      assertTrue(eventDetail.userVisits);
      assertTrue(eventDetail.actorVisits);
      assertFalse(userChip.hasAttribute('selected'));
      assertFalse(actorChip.hasAttribute('selected'));
    });

    test('CriticalActionsIcons', async () => {
      element.showActorFilter = true;
      loadTimeData.overrideValues({
        isCriticalActionsEnabled: true,
      });
      element.requestUpdate();
      await element.updateComplete;

      const userChip =
          element.shadowRoot.querySelector<HTMLElement>('#userVisitsChip');
      const actorChip =
          element.shadowRoot.querySelector<HTMLElement>('#actorVisitsChip');
      assertTrue(!!userChip && !!actorChip);

      const userIcon = userChip.querySelector('cr-icon');
      const actorIcon = actorChip.querySelector('cr-icon');
      assertTrue(!!userIcon && !!actorIcon);

      assertEquals('cr:person', userIcon.icon);

      // <if expr="_google_chrome">
      assertEquals('history-internal:arrow-selector-spark-new', actorIcon.icon);
      // </if>
      // <if expr="not _google_chrome">
      assertEquals('', actorIcon.icon);
      // </if>
    });
  });

  suite('DeviceFilter', function() {
    test('VisibilityAndInitialState', async () => {
      // If the parent does not pass `showDeviceFilter`, do not show the filter
      // by default.
      assertFalse(!!element.shadowRoot.querySelector('#deviceFilterChip'));

      element.devices = [
        {name: 'Device 1', isLocalDevice: true, ids: ['id1']},
        {name: 'Device 2', isLocalDevice: false, ids: ['id2']},
      ];
      element.showDeviceFilter = true;
      await microtasksFinished();
      const deviceChip = element.shadowRoot.querySelector<HTMLSelectElement>(
          '#deviceFilterChip');
      assertTrue(!!deviceChip);
      assertEquals(3, deviceChip.options.length);
      assertEquals(
          loadTimeData.getString('deviceFilterAllDevices'),
          deviceChip.options[0]!.textContent.trim());
      assertTrue(deviceChip.options[0]!.selected);
      assertEquals('Device 1', deviceChip.options[1]!.textContent.trim());
      assertFalse(deviceChip.options[1]!.selected);
      assertEquals('Device 2', deviceChip.options[2]!.textContent.trim());
      assertFalse(deviceChip.options[2]!.selected);

      const icon = element.shadowRoot.querySelector<HTMLElement>(
          '.filter-chips cr-icon');
      assertTrue(!!icon);
      assertFalse(isVisible(icon));

      element.showDeviceFilter = false;
      await microtasksFinished();
      assertFalse(!!element.shadowRoot.querySelector('#deviceFilterChip'));
    });

    test('SelectDeviceOptions', async () => {
      element.showDeviceFilter = true;
      element.devices = [
        {name: 'Device 1', isLocalDevice: true, ids: ['id1', 'id2']},
        {name: 'Device 2', isLocalDevice: false, ids: ['id3']},
      ];
      await microtasksFinished();
      const deviceChip = element.shadowRoot.querySelector<HTMLSelectElement>(
          '#deviceFilterChip');
      assertTrue(!!deviceChip);
      const icon = element.shadowRoot.querySelector<HTMLElement>(
          '.filter-chips cr-icon');
      assertTrue(!!icon);
      assertFalse(isVisible(icon));

      let eventDetail: {clientIds: string[]}|undefined;
      element.addEventListener('filter-changed', ((e: CustomEvent) => {
                                                   eventDetail = e.detail;
                                                 }) as EventListener);

      // Select 'Device 1'.
      deviceChip.value = '0';
      deviceChip.dispatchEvent(new Event('change'));
      await microtasksFinished();

      assertTrue(!!eventDetail);
      assertDeepEquals(['id1', 'id2'], eventDetail.clientIds);
      assertFalse(deviceChip.options[0]!.selected);
      assertTrue(deviceChip.options[1]!.selected);
      assertFalse(deviceChip.options[2]!.selected);
      assertTrue(isVisible(icon));
      eventDetail = undefined as {clientIds: string[]} | undefined;

      // Select 'All devices'.
      deviceChip.value = '-1';
      deviceChip.dispatchEvent(new Event('change'));
      await microtasksFinished();

      assertTrue(!!eventDetail);
      assertDeepEquals([], eventDetail.clientIds);
      assertTrue(deviceChip.options[0]!.selected);
      assertFalse(deviceChip.options[1]!.selected);
      assertFalse(deviceChip.options[2]!.selected);
      assertFalse(isVisible(icon));
      eventDetail = undefined as {clientIds: string[]} | undefined;

      // Select 'Device 2'.
      deviceChip.value = '1';
      deviceChip.dispatchEvent(new Event('change'));
      await microtasksFinished();

      assertTrue(!!eventDetail);
      assertDeepEquals(['id3'], eventDetail.clientIds);
      assertFalse(deviceChip.options[0]!.selected);
      assertFalse(deviceChip.options[1]!.selected);
      assertTrue(deviceChip.options[2]!.selected);
      assertTrue(isVisible(icon));
    });
  });

  test('ClearAllButton', async () => {
    loadTimeData.overrideValues({
      isBrowsingHistoryFilterByActorEnabled: true,
    });
    element.showActorFilter = true;
    element.showDeviceFilter = true;
    element.devices = [
      {name: 'Device 1', isLocalDevice: true, ids: ['id1', 'id2']},
      {name: 'Device 2', isLocalDevice: false, ids: ['id3']},
    ];
    await microtasksFinished();

    const actorFilterChip =
        element.shadowRoot.querySelector<HTMLSelectElement>('#actorFilterChip');
    const deviceFilterChip =
        element.shadowRoot.querySelector<HTMLSelectElement>(
            '#deviceFilterChip');
    assertTrue(!!actorFilterChip);
    assertTrue(!!deviceFilterChip);
    assertFalse(!!element.shadowRoot.querySelector('#clearAllButton'));

    // Selecting an actor filter shows the clear all button.
    actorFilterChip.value = 'user';
    actorFilterChip.dispatchEvent(new Event('change'));
    await microtasksFinished();
    assertTrue(!!element.shadowRoot.querySelector('#clearAllButton'));

    // Selecting a device keeps the clear all button visible.
    deviceFilterChip.value = '0';
    deviceFilterChip.dispatchEvent(new Event('change'));
    await microtasksFinished();
    assertTrue(!!element.shadowRoot.querySelector('#clearAllButton'));

    // Deselecting one filter while another is still active keeps the clear all
    // button visible.
    actorFilterChip.value = 'all';
    actorFilterChip.dispatchEvent(new Event('change'));
    await microtasksFinished();
    assertTrue(!!element.shadowRoot.querySelector('#clearAllButton'));

    // Deselecting the last active filter hides the clear all button.
    deviceFilterChip.value = '-1';
    deviceFilterChip.dispatchEvent(new Event('change'));
    await microtasksFinished();
    assertFalse(!!element.shadowRoot.querySelector('#clearAllButton'));

    // Clicking the clear all button resets all active filters and hides the
    // button.
    actorFilterChip.value = 'user';
    actorFilterChip.dispatchEvent(new Event('change'));
    deviceFilterChip.value = '0';
    deviceFilterChip.dispatchEvent(new Event('change'));
    await microtasksFinished();

    const clearAllButton =
        element.shadowRoot.querySelector<HTMLElement>('#clearAllButton');
    assertTrue(!!clearAllButton);

    let eventDetail: {
      userVisits: boolean,
      actorVisits: boolean,
      clientIds: string[],
    }|undefined;
    element.addEventListener('filter-changed', ((e: CustomEvent) => {
                                                 eventDetail = e.detail;
                                               }) as EventListener);

    clearAllButton.click();
    await microtasksFinished();

    assertFalse(!!element.shadowRoot.querySelector('#clearAllButton'));
    assertEquals('all', actorFilterChip.value);
    assertTrue(actorFilterChip.options[0]!.selected);
    assertEquals('-1', deviceFilterChip.value);
    assertTrue(deviceFilterChip.options[0]!.selected);

    assertTrue(!!eventDetail);
    assertTrue(eventDetail.userVisits);
    assertTrue(eventDetail.actorVisits);
    assertDeepEquals([], eventDetail.clientIds);
  });

  test('ClearAllButtonFocusesFirstAvailableFilter', async () => {
    loadTimeData.overrideValues({
      isBrowsingHistoryFilterByActorEnabled: true,
    });
    element.devices = [
      {name: 'Device 1', isLocalDevice: true, ids: ['id1', 'id2']},
      {name: 'Device 2', isLocalDevice: false, ids: ['id3']},
    ];
    element.showActorFilter = true;
    element.showDeviceFilter = true;
    await microtasksFinished();

    const actorFilterChip =
        element.shadowRoot.querySelector<HTMLSelectElement>('#actorFilterChip');
    const deviceFilterChip =
        element.shadowRoot.querySelector<HTMLSelectElement>(
            '#deviceFilterChip');
    assertTrue(!!actorFilterChip);
    assertTrue(!!deviceFilterChip);

    // When both filters are shown, focus moves to the actor filter (first).
    deviceFilterChip.value = '0';
    deviceFilterChip.dispatchEvent(new Event('change'));
    await microtasksFinished();

    let clearAllButton =
        element.shadowRoot.querySelector<HTMLElement>('#clearAllButton');
    assertTrue(!!clearAllButton);
    clearAllButton.focus();
    assertEquals(clearAllButton, element.shadowRoot.activeElement);
    clearAllButton.click();
    await microtasksFinished();

    assertEquals(actorFilterChip, element.shadowRoot.activeElement);

    // When the actor filter is hidden, focus moves to the device filter.
    element.showActorFilter = false;
    deviceFilterChip.value = '0';
    deviceFilterChip.dispatchEvent(new Event('change'));
    await microtasksFinished();

    clearAllButton =
        element.shadowRoot.querySelector<HTMLElement>('#clearAllButton');
    assertTrue(!!clearAllButton);
    clearAllButton.focus();
    assertEquals(clearAllButton, element.shadowRoot.activeElement);
    clearAllButton.click();
    await microtasksFinished();

    assertEquals(deviceFilterChip, element.shadowRoot.activeElement);
  });
});
