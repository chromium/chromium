// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {TabElement} from 'chrome://tab-strip/tab.js';
import {TabGroupElement} from 'chrome://tab-strip/tab_group.js';
import {setScrollAnimationEnabledForTesting, TabListElement} from 'chrome://tab-strip/tab_list.js';
import {TabStripEmbedderProxyImpl} from 'chrome://tab-strip/tab_strip_embedder_proxy.js';
import {TabData, TabsApiProxyImpl} from 'chrome://tab-strip/tabs_api_proxy.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {eventToPromise} from '../test_util.m.js';

import {TestTabStripEmbedderProxy} from './test_tab_strip_embedder_proxy.js';
import {TestTabsApiProxy} from './test_tabs_api_proxy.js';

suite('TabList', () => {
  /** @type {!TabListElement} */
  let tabList;

  /** @type {!TestTabStripEmbedderProxy} */
  let testTabStripEmbedderProxy;

  /** @type {!TestTabsApiProxy} */
  let testTabsApiProxy;

  /** @type {!Array<!TabData>} */
  const tabs = [
    {
      active: true,
      alertStates: [],
      id: 0,
      index: 0,
      pinned: false,
      title: 'Tab 1',
    },
    {
      active: false,
      alertStates: [],
      id: 1,
      index: 1,
      pinned: false,
      title: 'Tab 2',
    },
    {
      active: false,
      alertStates: [],
      id: 2,
      index: 2,
      pinned: false,
      title: 'Tab 3',
    },
  ];

  /**
   * @param {!TabData} tab
   * @param {number} index
   */
  function pinTabAt(tab, index) {
    const changeInfo = {index: index, pinned: true};
    const updatedTab = Object.assign({}, tab, changeInfo);
    webUIListenerCallback('tab-updated', updatedTab);
  }

  /**
   * @param {!TabData} tab
   * @param {number} index
   */
  function unpinTabAt(tab, index) {
    const changeInfo = {index: index, pinned: false};
    const updatedTab = Object.assign({}, tab, changeInfo);
    webUIListenerCallback('tab-updated', updatedTab);
  }

  /** @return {!NodeList<!TabElement>} */
  function getUnpinnedTabs() {
    return /** @type {!NodeList<!TabElement>} */ (
        tabList.shadowRoot.querySelectorAll('#unpinnedTabs tabstrip-tab'));
  }

  /** @return {!NodeList<!TabElement>} */
  function getPinnedTabs() {
    return /** @type {!NodeList<!TabElement>} */ (
        tabList.shadowRoot.querySelectorAll('#pinnedTabs tabstrip-tab'));
  }

  /** @return {!NodeList<!TabGroupElement>} */
  function getTabGroups() {
    return /** @type {!NodeList<!TabGroupElement>} */ (
        tabList.shadowRoot.querySelectorAll('tabstrip-tab-group'));
  }

  /**
   * @param {number} ms
   * @return {!Promise}
   */
  function waitFor(ms) {
    return new Promise(resolve => {
      setTimeout(resolve, ms);
    });
  }

  setup(async () => {
    document.documentElement.dir = 'ltr';
    document.body.innerHTML = '';
    document.body.style.margin = 0;

    testTabsApiProxy = new TestTabsApiProxy();
    testTabsApiProxy.setTabs(tabs);
    TabsApiProxyImpl.instance_ = testTabsApiProxy;

    testTabStripEmbedderProxy = new TestTabStripEmbedderProxy();
    testTabStripEmbedderProxy.setColors({
      '--background-color': 'white',
      '--foreground-color': 'black',
    });
    testTabStripEmbedderProxy.setLayout({
      '--height': '100px',
      '--width': '150px',
    });
    testTabStripEmbedderProxy.setVisible(true);
    TabStripEmbedderProxyImpl.instance_ = testTabStripEmbedderProxy;

    setScrollAnimationEnabledForTesting(false);

    tabList = /** @type {!TabListElement} */ (
        document.createElement('tabstrip-tab-list'));
    document.body.appendChild(tabList);

    await testTabsApiProxy.whenCalled('getTabs');
  });

  teardown(() => {
    testTabsApiProxy.reset();
    testTabStripEmbedderProxy.reset();
  });

  test('sets theme colors on init', async () => {
    await testTabStripEmbedderProxy.whenCalled('getColors');
    assertEquals(tabList.style.getPropertyValue('--background-color'), 'white');
    assertEquals(tabList.style.getPropertyValue('--foreground-color'), 'black');
  });

  test('updates theme colors when theme changes', async () => {
    testTabStripEmbedderProxy.setColors({
      '--background-color': 'pink',
      '--foreground-color': 'blue',
    });
    webUIListenerCallback('theme-changed');
    await testTabStripEmbedderProxy.whenCalled('getColors');
    assertEquals(tabList.style.getPropertyValue('--background-color'), 'pink');
    assertEquals(tabList.style.getPropertyValue('--foreground-color'), 'blue');
  });

  test('sets layout variables on init', async () => {
    await testTabStripEmbedderProxy.whenCalled('getLayout');
    assertEquals(tabList.style.getPropertyValue('--height'), '100px');
    assertEquals(tabList.style.getPropertyValue('--width'), '150px');
  });

  test('updates layout variables when layout changes', async () => {
    webUIListenerCallback('layout-changed', {
      '--height': '10000px',
      '--width': '10px',
    });
    await testTabStripEmbedderProxy.whenCalled('getLayout');
    assertEquals(tabList.style.getPropertyValue('--height'), '10000px');
    assertEquals(tabList.style.getPropertyValue('--width'), '10px');
  });

  test('GroupVisualDataOnInit', async () => {
    testTabsApiProxy.reset();
    testTabsApiProxy.setTabs([{
      active: true,
      alertStates: [],
      groupId: 'group0',
      id: 0,
      index: 0,
      title: 'New tab',
    }]);
    testTabsApiProxy.setGroupVisualData({
      group0: {
        title: 'My group',
        color: 'rgba(255, 0, 0, 1)',
      },
    });
    // Remove and reinsert into DOM to retrigger connectedCallback();
    tabList.remove();
    document.body.appendChild(tabList);
    await testTabsApiProxy.whenCalled('getGroupVisualData');
  });

  test('GroupVisualDataOnThemeChange', async () => {
    testTabsApiProxy.reset();
    testTabsApiProxy.setGroupVisualData({
      group0: {
        title: 'My group',
        color: 'rgba(255, 0, 0, 1)',
      },
    });
    webUIListenerCallback('theme-changed');
    await testTabsApiProxy.whenCalled('getGroupVisualData');
  });

  test('calculates the correct unpinned tab width and height', async () => {
    webUIListenerCallback('layout-changed', {
      '--tabstrip-tab-thumbnail-height': '132px',
      '--tabstrip-tab-thumbnail-width': '200px',
      '--tabstrip-tab-title-height': '15px',
    });
    await testTabStripEmbedderProxy.whenCalled('getLayout');

    const tabListStyle = window.getComputedStyle(tabList);
    assertEquals(
        tabListStyle.getPropertyValue('--tabstrip-tab-height').trim(),
        'calc(15px + 132px)');
    assertEquals(
        tabListStyle.getPropertyValue('--tabstrip-tab-width').trim(), '200px');
  });

  test('creates a tab element for each tab', () => {
    const tabElements = getUnpinnedTabs();
    assertEquals(tabs.length, tabElements.length);
    tabs.forEach((tab, index) => {
      assertEquals(tabElements[index].tab, tab);
    });
  });

  test('adds a new tab element when a tab is added in same window', () => {
    const appendedTab = {
      active: false,
      alertStates: [],
      id: 3,
      index: 3,
      title: 'New tab',
    };
    webUIListenerCallback('tab-created', appendedTab);
    let tabElements = getUnpinnedTabs();
    assertEquals(tabs.length + 1, tabElements.length);
    assertEquals(tabElements[tabs.length].tab, appendedTab);

    const prependedTab = {
      active: false,
      alertStates: [],
      id: 4,
      index: 0,
      title: 'New tab',
    };
    webUIListenerCallback('tab-created', prependedTab);
    tabElements = getUnpinnedTabs();
    assertEquals(tabs.length + 2, tabElements.length);
    assertEquals(tabElements[0].tab, prependedTab);
  });

  test('PlacesTabElement', () => {
    const pinnedTab =
        /** @type {!TabElement} */ (document.createElement('tabstrip-tab'));
    tabList.placeTabElement(pinnedTab, 0, true, undefined);
    assertEquals(pinnedTab, getPinnedTabs()[0]);

    const unpinnedUngroupedTab =
        /** @type {!TabElement} */ (document.createElement('tabstrip-tab'));
    tabList.placeTabElement(unpinnedUngroupedTab, 1, false, undefined);
    let unpinnedTabs = getUnpinnedTabs();
    assertEquals(4, unpinnedTabs.length);
    assertEquals(unpinnedUngroupedTab, unpinnedTabs[0]);

    const groupedTab =
        /** @type {!TabElement} */ (document.createElement('tabstrip-tab'));
    tabList.placeTabElement(groupedTab, 1, false, 'group0');
    unpinnedTabs = getUnpinnedTabs();
    assertEquals(5, unpinnedTabs.length);
    assertEquals(groupedTab, unpinnedTabs[0]);
    assertEquals('TABSTRIP-TAB-GROUP', groupedTab.parentElement.tagName);
  });

  /**
   * @param {!Element} element
   * @param {number} horizontalScale
   * @param {number} verticalScale
   */
  function testPlaceElementAnimationParams(
      element, horizontalScale, verticalScale) {
    const animations = element.getAnimations();
    assertEquals(1, animations.length);
    assertEquals('running', animations[0].playState);
    assertEquals(120, animations[0].effect.getTiming().duration);
    assertEquals('ease-out', animations[0].effect.getTiming().easing);

    const keyframes = animations[0].effect.getKeyframes();
    const horizontalTabSpacingVars =
        '(var(--tabstrip-tab-width) + var(--tabstrip-tab-spacing))';
    const verticalTabSpacingVars =
        '(var(--tabstrip-tab-height) + var(--tabstrip-tab-spacing))';
    assertEquals(2, keyframes.length);
    assertEquals(
        `translate(calc(${horizontalScale} * ${
            horizontalTabSpacingVars}), calc(${verticalScale} * ${
            verticalTabSpacingVars}))`,
        keyframes[0].transform);
    assertEquals('translate(0px, 0px)', keyframes[1].transform);
  }

  /**
   * This function should be called once per test since the animations finishing
   * and being included in the getAnimations() calls can cause flaky tests.
   * @param {number} indexToMove
   * @param {number} newIndex
   * @param {number} direction, the direction the moved tab should animate.
   *     +1 if moving right, -1 if moving left
   */
  async function testPlaceTabElementAnimation(
      indexToMove, newIndex, direction) {
    await tabList.animationPromises;
    let unpinnedTabs = getUnpinnedTabs();

    const movedTab = unpinnedTabs[indexToMove];
    tabList.placeTabElement(movedTab, newIndex, false, undefined);
    testPlaceElementAnimationParams(
        movedTab, -1 * direction * Math.abs(newIndex - indexToMove), 0);

    Array.from(unpinnedTabs)
        .filter(tabElement => tabElement !== movedTab)
        .forEach(
            tabElement =>
                testPlaceElementAnimationParams(tabElement, direction, 0));
  }

  test('PlaceTabElementAnimatesTabMovedTowardsStart', () => {
    return testPlaceTabElementAnimation(tabs.length - 1, 0, -1);
  });

  test('PlaceTabElementAnimatesTabMovedTowardsStartRTL', () => {
    document.documentElement.dir = 'rtl';
    return testPlaceTabElementAnimation(tabs.length - 1, 0, 1);
  });

  test('PlaceTabElementAnimatesTabMovedTowardsEnd', () => {
    return testPlaceTabElementAnimation(0, tabs.length - 1, 1);
  });

  test('PlaceTabElementAnimatesTabMovedTowardsEndRTL', () => {
    document.documentElement.dir = 'rtl';
    return testPlaceTabElementAnimation(0, tabs.length - 1, -1);
  });

  test('PlacePinnedTabElementAnimatesTabsWithinSameColumn', async () => {
    tabs.forEach(pinTabAt);
    await tabList.animationPromises;

    // Test moving a tab within the same column. If a tab is moved from index 0
    // to index 2, it should move vertically down 2 places. Tabs at index 1 and
    // index 2 should move up 1 space.
    const pinnedTabs = getPinnedTabs();
    tabList.placeTabElement(pinnedTabs[0], 2, /*pinned=*/ true);
    await Promise.all([
      testPlaceElementAnimationParams(pinnedTabs[0], 0, -2),
      testPlaceElementAnimationParams(pinnedTabs[1], 0, 1),
      testPlaceElementAnimationParams(pinnedTabs[2], 0, 1),
    ]);
  });

  test(
      'PlacePinnedTabElementAnimatesTabsAcrossColumnsToHigherIndex',
      async () => {
        tabs.forEach(pinTabAt);
        for (let i = 0; i < 4; i++) {
          webUIListenerCallback('tab-created', {
            active: false,
            alertStates: [],
            id: tabs.length + i,
            index: tabs.length + i,
            pinned: true,
            title: 'Pinned tab',
          });
        }
        await tabList.animationPromises;

        const pinnedTabs = getPinnedTabs();
        tabList.placeTabElement(pinnedTabs[2], 6, /*pinned=*/ true);
        await Promise.all([
          testPlaceElementAnimationParams(pinnedTabs[2], -2, 2),
          testPlaceElementAnimationParams(pinnedTabs[3], 1, -2),
          testPlaceElementAnimationParams(pinnedTabs[4], 0, 1),
          testPlaceElementAnimationParams(pinnedTabs[5], 0, 1),
          testPlaceElementAnimationParams(pinnedTabs[6], 1, -2),
        ]);
      });

  test(
      'PlacePinnedTabElementAnimatesTabsAcrossColumnsToLowerIndex',
      async () => {
        tabs.forEach(pinTabAt);
        for (let i = 0; i < 4; i++) {
          webUIListenerCallback('tab-created', {
            active: false,
            alertStates: [],
            id: tabs.length + i,
            index: tabs.length + i,
            pinned: true,
            title: 'Pinned tab',
          });
        }
        await tabList.animationPromises;

        const pinnedTabs = getPinnedTabs();
        tabList.placeTabElement(pinnedTabs[3], 0, /*pinned=*/ true);
        await Promise.all([
          testPlaceElementAnimationParams(pinnedTabs[3], 1, 0),
          testPlaceElementAnimationParams(pinnedTabs[2], -1, 2),
          testPlaceElementAnimationParams(pinnedTabs[1], 0, -1),
          testPlaceElementAnimationParams(pinnedTabs[0], 0, -1),
        ]);
      });

  test('PlacesTabGroupElement', () => {
    const tabGroupElement = /** @type {!TabGroupElement} */ (
        document.createElement('tabstrip-tab-group'));
    tabList.placeTabGroupElement(tabGroupElement, 2);

    const tabGroupElements = getTabGroups();
    assertEquals(1, tabGroupElements.length);
    assertEquals(tabGroupElement, tabGroupElements[0]);

    // Group was inserted at index 2, so it should come after the 2nd tab.
    assertEquals(getUnpinnedTabs()[1], tabGroupElement.previousElementSibling);
  });

  /**
   * @param {number} indexToGroup
   * @param {number} newIndex
   * @param {number} direction
   */
  async function testPlaceTabGroupElementAnimation(
      indexToGroup, newIndex, direction) {
    await tabList.animationPromises;

    // Group the tab at indexToGroup.
    const unpinnedTabs = getUnpinnedTabs();
    const tabToGroup = unpinnedTabs[indexToGroup];
    webUIListenerCallback(
        'tab-group-state-changed', tabToGroup.tab.id, indexToGroup, 'group0');

    const groupElement =
        /** @type {!TabGroupElement} */ (tabToGroup.parentElement);
    tabList.placeTabGroupElement(groupElement, newIndex);
    testPlaceElementAnimationParams(
        groupElement, -1 * direction * Math.abs(newIndex - indexToGroup), 0);

    // Test animations on all the other tabs.
    Array.from(getUnpinnedTabs())
        .filter(tabElement => tabElement.parentElement !== groupElement)
        .forEach(
            tabElement =>
                testPlaceElementAnimationParams(tabElement, direction, 0));
  }

  test('PlaceTabGroupElementAnimatesTabGroupMovedTowardsStart', () => {
    return testPlaceTabGroupElementAnimation(tabs.length - 1, 0, -1);
  });

  test('PlaceTabGroupElementAnimatesTabGroupMovedTowardsStartRTL', () => {
    document.documentElement.dir = 'rtl';
    return testPlaceTabGroupElementAnimation(tabs.length - 1, 0, 1);
  });

  test('PlaceTabGroupElementAnimatesTabGroupMovedTowardsEnd', () => {
    return testPlaceTabGroupElementAnimation(0, tabs.length - 1, 1);
  });

  test('PlaceTabGroupElementAnimatesTabGroupMovedTowardsEndRTL', () => {
    document.documentElement.dir = 'rtl';
    return testPlaceTabGroupElementAnimation(0, tabs.length - 1, -1);
  });

  test('PlaceTabGroupElementAnimationWithMultipleTabs', async () => {
    await tabList.animationPromises;

    // Group all tabs except for the first one.
    const ungroupedTab = getUnpinnedTabs()[0];
    tabs.slice(1).forEach(tab => {
      webUIListenerCallback(
          'tab-group-state-changed', tab.id, tab.index, 'group0');
    });

    // Move the group to index 0.
    const tabGroup = getTabGroups()[0];
    tabList.placeTabGroupElement(tabGroup, 0);

    // Both the TabElement and TabGroupElement should move by a scale of 1.
    testPlaceElementAnimationParams(tabGroup, 1, 0);
    testPlaceElementAnimationParams(ungroupedTab, -1, 0);
  });

  test('AddNewTabGroup', () => {
    const appendedTab = {
      active: false,
      alertStates: [],
      groupId: 'group0',
      id: 3,
      index: 3,
      title: 'New tab in group',
    };
    webUIListenerCallback('tab-created', appendedTab);
    let tabElements = getUnpinnedTabs();
    assertEquals(tabs.length + 1, tabElements.length);
    assertEquals(getTabGroups().length, 1);
    assertEquals(
        'TABSTRIP-TAB-GROUP',
        tabElements[appendedTab.index].parentElement.tagName);

    const prependedTab = {
      active: false,
      alertStates: [],
      groupId: 'group1',
      id: 4,
      index: 0,
      title: 'New tab',
    };
    webUIListenerCallback('tab-created', prependedTab);
    tabElements = getUnpinnedTabs();
    assertEquals(tabs.length + 2, tabElements.length);
    assertEquals(getTabGroups().length, 2);
    assertEquals(
        'TABSTRIP-TAB-GROUP',
        tabElements[prependedTab.index].parentElement.tagName);
  });

  test('AddTabToExistingGroup', () => {
    const appendedTab = {
      active: false,
      alertStates: [],
      groupId: 'group0',
      id: 3,
      index: 3,
      title: 'New tab in group',
    };
    webUIListenerCallback('tab-created', appendedTab);
    const appendedTabInSameGroup = {
      active: false,
      alertStates: [],
      groupId: 'group0',
      id: 4,
      index: 4,
      title: 'New tab in same group',
    };
    webUIListenerCallback('tab-created', appendedTabInSameGroup);
    const tabGroups = getTabGroups();
    assertEquals(tabGroups.length, 1);
    assertEquals(tabGroups[0].children.item(0).tab.id, appendedTab.id);
    assertEquals(
        tabGroups[0].children.item(1).tab.id, appendedTabInSameGroup.id);
  });

  // Test that the TabList does not add a non-grouped tab to a tab group at the
  // same index.
  test('HandleSingleTabBeforeGroup', () => {
    const tabInGroup = {
      active: false,
      alertStates: [],
      groupId: 'group0',
      id: 3,
      index: 3,
      title: 'New tab in group',
    };
    webUIListenerCallback('tab-created', tabInGroup);
    const tabNotInGroup = {
      active: false,
      alertStates: [],
      id: 4,
      index: 3,
      title: 'New tab not in group',
    };
    webUIListenerCallback('tab-created', tabNotInGroup);
    const tabsContainerChildren =
        tabList.shadowRoot.querySelector('#unpinnedTabs').children;
    assertEquals(tabsContainerChildren.item(3).tagName, 'TABSTRIP-TAB');
    assertEquals(tabsContainerChildren.item(3).tab, tabNotInGroup);
    assertEquals(tabsContainerChildren.item(4).tagName, 'TABSTRIP-TAB-GROUP');
  });

  test('HandleGroupedTabBeforeDifferentGroup', () => {
    const tabInOriginalGroup = tabs[1];
    webUIListenerCallback(
        'tab-group-state-changed', tabInOriginalGroup.id,
        tabInOriginalGroup.index, 'originalGroup');

    // Create another group from the tab before group A.
    const tabInPrecedingGroup = tabs[0];
    webUIListenerCallback(
        'tab-group-state-changed', tabInPrecedingGroup.id,
        tabInPrecedingGroup.index, 'precedingGroup');
    const tabsContainerChildren =
        tabList.shadowRoot.querySelector('#unpinnedTabs').children;

    const precedingGroup = tabsContainerChildren[0];
    assertEquals(precedingGroup.tagName, 'TABSTRIP-TAB-GROUP');
    assertEquals(precedingGroup.dataset.groupId, 'precedingGroup');
    assertEquals(precedingGroup.children.length, 1);
    assertEquals(precedingGroup.children[0].tab.id, tabInPrecedingGroup.id);

    const originalGroup = tabsContainerChildren[1];
    assertEquals(originalGroup.tagName, 'TABSTRIP-TAB-GROUP');
    assertEquals(originalGroup.dataset.groupId, 'originalGroup');
    assertEquals(originalGroup.children.length, 1);
    assertEquals(originalGroup.children[0].tab.id, tabInOriginalGroup.id);
  });

  test('HandleGroupedTabBeforeSameGroup', () => {
    const originalTabInGroup = tabs[1];
    webUIListenerCallback(
        'tab-group-state-changed', originalTabInGroup.id,
        originalTabInGroup.index, 'sameGroup');

    // Create another group from the tab before group A.
    const precedingTabInGroup = tabs[0];
    webUIListenerCallback(
        'tab-group-state-changed', precedingTabInGroup.id,
        precedingTabInGroup.index, 'sameGroup');

    const tabGroups = getTabGroups();
    const tabGroup = tabGroups[0];
    assertEquals(tabGroups.length, 1);
    assertEquals(tabGroup.dataset.groupId, 'sameGroup');
    assertEquals(tabGroup.children.length, 2);
    assertEquals(tabGroup.children[0].tab.id, precedingTabInGroup.id);
    assertEquals(tabGroup.children[1].tab.id, originalTabInGroup.id);
  });

  test('removes a tab when tab is removed from current window', async () => {
    const tabToRemove = tabs[0];
    webUIListenerCallback('tab-removed', tabToRemove.id);
    await tabList.animationPromises;
    assertEquals(tabs.length - 1, getUnpinnedTabs().length);
  });

  test('updates a tab with new tab data when a tab is updated', () => {
    const tabToUpdate = tabs[0];
    const changeInfo = {title: 'A new title'};
    const updatedTab = Object.assign({}, tabToUpdate, changeInfo);
    webUIListenerCallback('tab-updated', updatedTab);
    const tabElements = getUnpinnedTabs();
    assertEquals(tabElements[0].tab, updatedTab);
  });

  test('updates tabs when a new tab is activated', () => {
    const tabElements = getUnpinnedTabs();

    // Mock activating the 2nd tab
    webUIListenerCallback('tab-active-changed', tabs[1].id);
    assertFalse(tabElements[0].tab.active);
    assertTrue(tabElements[1].tab.active);
    assertFalse(tabElements[2].tab.active);
  });

  test('adds a pinned tab to its designated container', () => {
    webUIListenerCallback('tab-created', {
      active: false,
      alertStates: [],
      index: 0,
      title: 'New pinned tab',
      pinned: true,
    });
    const pinnedTabElements = getPinnedTabs();
    assertEquals(pinnedTabElements.length, 1);
    assertTrue(pinnedTabElements[0].tab.pinned);
  });

  test('moves pinned tabs to designated containers', () => {
    const tabToPin = tabs[1];
    const changeInfo = {index: 0, pinned: true};
    let updatedTab = Object.assign({}, tabToPin, changeInfo);
    webUIListenerCallback('tab-updated', updatedTab);

    let pinnedTabElements = getPinnedTabs();
    assertEquals(pinnedTabElements.length, 1);
    assertTrue(pinnedTabElements[0].tab.pinned);
    assertEquals(pinnedTabElements[0].tab.id, tabToPin.id);
    assertEquals(getUnpinnedTabs().length, 2);

    // Unpin the tab so that it's now at index 0
    changeInfo.index = 0;
    changeInfo.pinned = false;
    updatedTab = Object.assign({}, updatedTab, changeInfo);
    webUIListenerCallback('tab-updated', updatedTab);

    const unpinnedTabElements = getUnpinnedTabs();
    assertEquals(getPinnedTabs().length, 0);
    assertEquals(unpinnedTabElements.length, 3);
    assertEquals(unpinnedTabElements[0].tab.id, tabToPin.id);
  });

  test('moves tab elements when tabs move', () => {
    const tabElementsBeforeMove = getUnpinnedTabs();
    const tabToMove = tabs[0];
    webUIListenerCallback('tab-moved', tabToMove.id, 2);

    const tabElementsAfterMove = getUnpinnedTabs();
    assertEquals(tabElementsBeforeMove[0], tabElementsAfterMove[2]);
    assertEquals(tabElementsBeforeMove[1], tabElementsAfterMove[0]);
    assertEquals(tabElementsBeforeMove[2], tabElementsAfterMove[1]);
  });

  test('MoveExistingTabToGroup', () => {
    const tabToGroup = tabs[1];
    webUIListenerCallback(
        'tab-group-state-changed', tabToGroup.id, tabToGroup.index, 'group0');
    let tabElements = getUnpinnedTabs();
    assertEquals(tabElements.length, tabs.length);
    assertEquals(
        tabElements[tabToGroup.index].parentElement.tagName,
        'TABSTRIP-TAB-GROUP');

    const anotherTabToGroup = tabs[2];
    webUIListenerCallback(
        'tab-group-state-changed', anotherTabToGroup.id,
        anotherTabToGroup.index, 'group0');
    tabElements = getUnpinnedTabs();
    assertEquals(tabElements.length, tabs.length);
    assertEquals(
        tabElements[tabToGroup.index].parentElement,
        tabElements[anotherTabToGroup.index].parentElement);
  });

  test('MoveTabGroup', () => {
    const tabToGroup = tabs[1];
    webUIListenerCallback(
        'tab-group-state-changed', tabToGroup.id, tabToGroup.index, 'group0');
    webUIListenerCallback('tab-group-moved', 'group0', 0);

    const tabAtIndex0 = getUnpinnedTabs()[0];
    assertEquals(tabAtIndex0.parentElement.tagName, 'TABSTRIP-TAB-GROUP');
    assertEquals(tabAtIndex0.tab.id, tabToGroup.id);
  });

  test('tracks and untracks thumbnails based on viewport', async () => {
    // Wait for slideIn animations to complete updating widths and reset
    // resolvers to track new calls.
    await tabList.animationPromises;
    await testTabsApiProxy.whenCalled('setThumbnailTracked');
    testTabsApiProxy.reset();
    const tabElements = getUnpinnedTabs();

    // Update width such that at most one tab can fit in the viewport at once.
    tabList.style.setProperty('--tabstrip-tab-width', `${window.innerWidth}px`);

    // At this point, the only visible tab should be the first tab. The second
    // tab should fit within the rootMargin of the IntersectionObserver. The
    // third tab should not be intersecting.
    let [tabId, thumbnailTracked] =
        await testTabsApiProxy.whenCalled('setThumbnailTracked');
    assertEquals(tabId, tabElements[2].tab.id);
    assertEquals(thumbnailTracked, false);
    assertEquals(testTabsApiProxy.getCallCount('setThumbnailTracked'), 1);
    testTabsApiProxy.reset();

    // Scroll such that the second tab is now the only visible tab. At this
    // point, all 3 tabs should fit within the root and rootMargin of the
    // IntersectionObserver. Since the 3rd tab was not being tracked before,
    // it should be the only tab to become tracked.
    tabList.scrollLeft = tabElements[1].offsetLeft;
    [tabId, thumbnailTracked] =
        await testTabsApiProxy.whenCalled('setThumbnailTracked');
    assertEquals(tabId, tabElements[2].tab.id);
    assertEquals(thumbnailTracked, true);
    assertEquals(testTabsApiProxy.getCallCount('setThumbnailTracked'), 1);
    testTabsApiProxy.reset();

    // Scroll such that the third tab is now the only visible tab. At this
    // point, the first tab should be outside of the rootMargin of the
    // IntersectionObserver.
    tabList.scrollLeft = tabElements[2].offsetLeft;
    [tabId, thumbnailTracked] =
        await testTabsApiProxy.whenCalled('setThumbnailTracked');
    assertEquals(tabId, tabElements[0].tab.id);
    assertEquals(thumbnailTracked, false);
    assertEquals(testTabsApiProxy.getCallCount('setThumbnailTracked'), 1);
  });

  test('tracks and untracks thumbnails based on pinned state', async () => {
    await tabList.animationPromises;
    await testTabsApiProxy.whenCalled('setThumbnailTracked');
    testTabsApiProxy.reset();

    // Remove all other tabs to isolate the tab to test, and then wait for
    // each tab to get untracked as it is removed from the DOM.
    const tabElements = getUnpinnedTabs();
    tabElements[0].remove();
    await testTabsApiProxy.whenCalled('setThumbnailTracked');
    testTabsApiProxy.reset();
    tabElements[1].remove();
    await testTabsApiProxy.whenCalled('setThumbnailTracked');
    testTabsApiProxy.reset();

    // Pinning the third tab should untrack thumbnails for the tab
    pinTabAt(tabs[2], 0);
    let [tabId, thumbnailTracked] =
        await testTabsApiProxy.whenCalled('setThumbnailTracked');
    assertEquals(tabId, tabs[2].id);
    assertEquals(thumbnailTracked, false);
    testTabsApiProxy.reset();

    // Unpinning the tab should re-track the thumbnails
    unpinTabAt(tabs[2], 0);
    [tabId, thumbnailTracked] =
        await testTabsApiProxy.whenCalled('setThumbnailTracked');

    assertEquals(tabId, tabs[2].id);
    assertEquals(thumbnailTracked, true);
  });

  test('should update thumbnail track status on visibilitychange', async () => {
    await tabList.animationPromises;
    await testTabsApiProxy.whenCalled('setThumbnailTracked');
    testTabsApiProxy.reset();

    testTabStripEmbedderProxy.setVisible(false);
    document.dispatchEvent(new Event('visibilitychange'));

    // The tab strip should force untrack thumbnails for all tabs.
    await testTabsApiProxy.whenCalled('setThumbnailTracked');
    assertEquals(
        testTabsApiProxy.getCallCount('setThumbnailTracked'), tabs.length);
    testTabsApiProxy.reset();

    // Update width such that at all tabs can fit
    tabList.style.setProperty(
        '--tabstrip-tab-width', `${window.innerWidth / tabs.length}px`);

    testTabStripEmbedderProxy.setVisible(true);
    document.dispatchEvent(new Event('visibilitychange'));

    await testTabsApiProxy.whenCalled('setThumbnailTracked');
    assertEquals(
        testTabsApiProxy.getCallCount('setThumbnailTracked'), tabs.length);
  });

  test('ShouldDebounceThumbnailTrackerWhenScrollingFast', async () => {
    // Set tab widths such that 3 tabs fit in the viewport. This should reach a
    // state where the first 6 thumbnails are being tracked: 3 in the viewport
    // and 3 within the IntersectionObserver's rootMargin. The widths need to be
    // full integers to avoid rounding errors.
    const tabsPerViewport = 3;
    const tabStripWidth = window.innerWidth - window.innerWidth % 3;
    tabList.style.width = `${tabStripWidth}px`;
    tabList.style.setProperty(
        '--tabstrip-tab-width', `${tabStripWidth / tabsPerViewport}px`);
    tabList.style.setProperty('--tabstrip-tab-height', '10px');
    tabList.style.setProperty('--tabstrip-tab-spacing', '0px');

    await tabList.animationPromises;
    await testTabsApiProxy.whenCalled('setThumbnailTracked');
    testTabsApiProxy.reset();

    // Add enough tabs for there to be 13 tabs.
    for (let i = 0; i < 10; i++) {
      webUIListenerCallback('tab-created', {
        active: false,
        alertStates: [],
        id: tabs.length + i,
        index: tabs.length + i,
        title: '',
      });
    }
    await tabList.animationPromises;
    await testTabsApiProxy.whenCalled('setThumbnailTracked');
    testTabsApiProxy.reset();
    testTabsApiProxy.resetThumbnailRequestCounts();

    // Mock 3 scroll events and end up with a scrolled state where the 10th
    // tab is aligned to the left. This should only evaluate to 1 set of
    // thumbnail updates and should most importantly skip the 6th tab.
    const tabElements = getUnpinnedTabs();
    tabList.scrollLeft = tabElements[3].offsetLeft;
    tabList.scrollLeft = tabElements[5].offsetLeft;
    tabList.scrollLeft = tabElements[10].offsetLeft;
    assertEquals(0, testTabsApiProxy.getCallCount('setThumbnailTracked'));

    await waitFor(200);
    assertEquals(12, testTabsApiProxy.getCallCount('setThumbnailTracked'));
    assertEquals(0, testTabsApiProxy.getThumbnailRequestCount(6));
    for (let tabId = 7; tabId < 13; tabId++) {
      assertEquals(1, testTabsApiProxy.getThumbnailRequestCount(tabId));
    }
  });

  test(
      'focusing on tab strip with the keyboard adds a class and focuses ' +
          'the first tab',
      () => {
        webUIListenerCallback('received-keyboard-focus');
        assertEquals(document.activeElement, tabList);
        assertEquals(tabList.shadowRoot.activeElement, getUnpinnedTabs()[0]);
        assertTrue(FocusOutlineManager.forDocument(document).visible);
      });

  test('blurring the tab strip blurs the active element', () => {
    // First, make sure tab strip has keyboard focus.
    webUIListenerCallback('received-keyboard-focus');

    window.dispatchEvent(new Event('blur'));
    assertEquals(tabList.shadowRoot.activeElement, null);
  });

  test('should update the ID when a tab is replaced', () => {
    assertEquals(getUnpinnedTabs()[0].tab.id, 0);
    webUIListenerCallback('tab-replaced', tabs[0].id, 1000);
    assertEquals(getUnpinnedTabs()[0].tab.id, 1000);
  });

  test('has custom context menu', async () => {
    let event = new Event('contextmenu');
    event.clientX = 1;
    event.clientY = 2;
    document.dispatchEvent(event);

    const contextMenuArgs =
        await testTabStripEmbedderProxy.whenCalled('showBackgroundContextMenu');
    assertEquals(contextMenuArgs[0], 1);
    assertEquals(contextMenuArgs[1], 2);
  });

  test('scrolls to active tabs', async () => {
    await tabList.animationPromises;

    const newTabButtonMargin = 15;
    const newTabButtonWidth = 50;
    const scrollPadding = 32;
    const tabWidth = 200;
    const viewportWidth = 300;

    // Mock the width of each tab element.
    tabList.style.setProperty(
        '--tabstrip-new-tab-button-margin', `${newTabButtonMargin}px`);
    tabList.style.setProperty(
        '--tabstrip-new-tab-button-width', `${newTabButtonWidth}px`);
    tabList.style.setProperty(
        '--tabstrip-tab-thumbnail-width', `${tabWidth}px`);
    tabList.style.setProperty('--tabstrip-tab-spacing', '0px');
    const tabElements = getUnpinnedTabs();
    tabElements.forEach(tabElement => {
      tabElement.style.width = `${tabWidth}px`;
    });

    // Mock the scroller size such that it cannot fit only 1 tab at a time.
    tabList.style.setProperty(
        '--tabstrip-viewport-width', `${viewportWidth}px`);
    tabList.style.width = `${viewportWidth}px`;

    // Verify the scrollLeft is currently at its default state of 0, and then
    // send a visibilitychange event to cause a scroll.
    assertEquals(tabList.scrollLeft, 0);
    webUIListenerCallback('tab-active-changed', tabs[1].id);
    testTabStripEmbedderProxy.setVisible(false);
    document.dispatchEvent(new Event('visibilitychange'));

    // The 2nd tab should be off-screen to the right, so activating it should
    // scroll so that the element's right edge is aligned with the screen's
    // right edge.
    let activeTab = getUnpinnedTabs()[1];
    assertEquals(
        tabList.scrollLeft + tabList.offsetWidth,
        activeTab.offsetLeft + activeTab.offsetWidth + scrollPadding +
            newTabButtonMargin + newTabButtonWidth);

    // The 1st tab should be now off-screen to the left, so activating it should
    // scroll so that the element's left edge is aligned with the screen's
    // left edge.
    webUIListenerCallback('tab-active-changed', tabs[0].id);
    activeTab = getUnpinnedTabs()[0];
    assertEquals(tabList.scrollLeft, 0);
  });

  test('clicking on new tab button opens a new tab', async () => {
    tabList.shadowRoot.querySelector('#newTabButton').click();
    await testTabsApiProxy.whenCalled('createNewTab');
  });

  test('PreventsDraggingWhenOnlyOneTab', () => {
    assertFalse(tabList.shouldPreventDrag());
    const tabElements = getUnpinnedTabs();
    tabElements[1].remove();
    tabElements[2].remove();
    assertTrue(tabList.shouldPreventDrag());
  });
});
