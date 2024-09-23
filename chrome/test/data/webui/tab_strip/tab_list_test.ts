// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-strip.top-chrome/tab_list.js';

import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import type {TabElement} from 'chrome://tab-strip.top-chrome/tab.js';
import type {TabGroupElement} from 'chrome://tab-strip.top-chrome/tab_group.js';
import type {TabListElement} from 'chrome://tab-strip.top-chrome/tab_list.js';
import {setScrollAnimationEnabledForTesting} from 'chrome://tab-strip.top-chrome/tab_list.js';
import type {PageRemote, Tab} from 'chrome://tab-strip.top-chrome/tab_strip.mojom-webui.js';
import {TabsApiProxyImpl} from 'chrome://tab-strip.top-chrome/tabs_api_proxy.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createTab, TestTabsApiProxy} from './test_tabs_api_proxy.js';

function flushTasks() {
  return new Promise(function(resolve) {
    window.setTimeout(resolve, 1);
  });
}

suite('TabList', () => {
  let tabList: TabListElement;
  let testTabsApiProxy: TestTabsApiProxy;
  let callbackRouter: PageRemote;

  const tabs: Tab[] = [
    createTab({
      active: true,
      id: 0,
      index: 0,
      title: 'Tab 1',
    }),
    createTab({
      id: 1,
      index: 1,
      title: 'Tab 2',
    }),
    createTab({
      active: false,
      id: 2,
      index: 2,
      title: 'Tab 3',
    }),
  ];

  function pinTabAt(tab: Tab, index: number) {
    const changeInfo = {index: index, pinned: true};
    const updatedTab = Object.assign({}, tab, changeInfo);
    callbackRouter.tabUpdated(updatedTab);
  }

  function unpinTabAt(tab: Tab, index: number) {
    const changeInfo = {index: index, pinned: false};
    const updatedTab = Object.assign({}, tab, changeInfo);
    callbackRouter.tabUpdated(updatedTab);
  }

  function getUnpinnedTabs(): NodeListOf<TabElement> {
    return tabList.shadowRoot!.querySelectorAll('#unpinnedTabs tabstrip-tab');
  }

  function getUnpinnedActiveTabs(): NodeListOf<TabElement> {
    return tabList.shadowRoot!.querySelectorAll(
        '#unpinnedTabs tabstrip-tab[active]');
  }

  function getPinnedTabs(): NodeListOf<TabElement> {
    return tabList.shadowRoot!.querySelectorAll('#pinnedTabs tabstrip-tab');
  }

  function getTabGroups(): NodeListOf<TabGroupElement> {
    return tabList.shadowRoot!.querySelectorAll('tabstrip-tab-group');
  }

  function waitFor(ms: number): Promise<void> {
    return new Promise<void>(resolve => {
      setTimeout(resolve, ms);
    });
  }

  function verifyTab(tab1: Tab, tab2: Tab) {
    assertEquals(tab1.active, tab2.active);
    assertDeepEquals(tab1.alertStates, tab2.alertStates);
    assertEquals(tab1.id, tab2.id);
    assertEquals(tab1.index, tab2.index);
    assertEquals(tab1.pinned, tab2.pinned);
    assertEquals(tab1.title, tab2.title);
    assertDeepEquals(tab1.url, tab2.url);
  }

  setup(async () => {
    document.documentElement.dir = 'ltr';
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.style.margin = '0';

    testTabsApiProxy = new TestTabsApiProxy();
    testTabsApiProxy.setTabs(tabs);
    TabsApiProxyImpl.setInstance(testTabsApiProxy);
    callbackRouter = testTabsApiProxy.getCallbackRouterRemote();

    testTabsApiProxy.setLayout({
      '--height': '100px',
      '--width': '150px',
    });
    testTabsApiProxy.setVisible(true);

    setScrollAnimationEnabledForTesting(false);

    tabList = document.createElement('tabstrip-tab-list');
    document.body.appendChild(tabList);

    await testTabsApiProxy.whenCalled('getTabs');
  });

  teardown(() => {
    testTabsApiProxy.reset();
  });

  test('sets layout variables on init', async () => {
    await testTabsApiProxy.whenCalled('getLayout');
    assertEquals(tabList.style.getPropertyValue('--height'), '100px');
    assertEquals(tabList.style.getPropertyValue('--width'), '150px');
  });

  test('updates layout variables when layout changes', async () => {
    callbackRouter.layoutChanged({
      '--height': '10000px',
      '--width': '10px',
    });
    await flushTasks();
    await testTabsApiProxy.whenCalled('getLayout');
    assertEquals(tabList.style.getPropertyValue('--height'), '10000px');
    assertEquals(tabList.style.getPropertyValue('--width'), '10px');
  });

  test('GroupVisualDataOnInit', async () => {
    testTabsApiProxy.reset();
    testTabsApiProxy.setTabs([createTab({
      active: true,
      groupId: 'group0',
      id: 0,
      index: 0,
      title: 'New tab',
    })]);
    testTabsApiProxy.setGroupVisualData({
      group0: {
        title: 'My group',
        color: 'rgba(255, 0, 0, 1)',
        textColor: 'black',
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
        textColor: 'black',
      },
    });
    callbackRouter.themeChanged();
    await flushTasks();
    await testTabsApiProxy.whenCalled('getGroupVisualData');
  });

  test('calculates the correct unpinned tab width and height', async () => {
    callbackRouter.layoutChanged({
      '--tabstrip-tab-thumbnail-height': '132px',
      '--tabstrip-tab-thumbnail-width': '200px',
      '--tabstrip-tab-title-height': '15px',
    });
    await flushTasks();
    await testTabsApiProxy.whenCalled('getLayout');

    const tabListStyle = window.getComputedStyle(tabList);
    assertEquals(
        tabListStyle.getPropertyValue('--tabstrip-tab-height')
            .trim()
            .replace(/\n */, ' '),
        'calc(15px + 132px)');
    assertEquals(
        tabListStyle.getPropertyValue('--tabstrip-tab-width').trim(), '200px');
  });

  test('creates a tab element for each tab', () => {
    const tabElements = getUnpinnedTabs();
    assertEquals(tabs.length, tabElements.length);
    tabs.forEach((tab, index) => {
      assertEquals(tabElements[index]!.tab, tab);
    });
  });

  test(
      'adds a new tab element when a tab is added in same window', async () => {
        const appendedTab = createTab({
          active: false,
          id: 3,
          index: 3,
          title: 'New tab',
        });
        callbackRouter.tabCreated(appendedTab);
        await flushTasks();
        let tabElements = getUnpinnedTabs();
        assertEquals(tabs.length + 1, tabElements.length);
        verifyTab(tabElements[tabs.length]!.tab, appendedTab);

        const prependedTab = createTab({
          id: 4,
          index: 0,
          title: 'New tab',
        });
        callbackRouter.tabCreated(prependedTab);
        await flushTasks();
        tabElements = getUnpinnedTabs();
        assertEquals(tabs.length + 2, tabElements.length);
        verifyTab(tabElements[0]!.tab, prependedTab);
      });

  test('PlacesTabElement', () => {
    const pinnedTab = document.createElement('tabstrip-tab');
    tabList.placeTabElement(pinnedTab, 0, true, null);
    assertEquals(pinnedTab, getPinnedTabs()[0]);

    const unpinnedUngroupedTab = document.createElement('tabstrip-tab');
    tabList.placeTabElement(unpinnedUngroupedTab, 1, false, null);
    let unpinnedTabs = getUnpinnedTabs();
    assertEquals(4, unpinnedTabs.length);
    assertEquals(unpinnedUngroupedTab, unpinnedTabs[0]);

    const groupedTab = document.createElement('tabstrip-tab');
    tabList.placeTabElement(groupedTab, 1, false, 'group0');
    unpinnedTabs = getUnpinnedTabs();
    assertEquals(5, unpinnedTabs.length);
    assertEquals(groupedTab, unpinnedTabs[0]);
    assertEquals('TABSTRIP-TAB-GROUP', groupedTab.parentElement!.tagName);
  });

  function testPlaceElementAnimationParams(
      element: Element, horizontalScale: number, verticalScale: number) {
    const animations = element.getAnimations();
    assertEquals(1, animations.length);
    assertEquals('running', animations[0]!.playState);
    assertEquals(120, animations[0]!.effect!.getTiming().duration);
    assertEquals('ease-out', animations[0]!.effect!.getTiming().easing);

    const keyframes = (animations[0]!.effect as KeyframeEffect).getKeyframes();
    const horizontalTabSpacingVars =
        '(var(--tabstrip-tab-width) + var(--tabstrip-tab-spacing))';
    const verticalTabSpacingVars =
        '(var(--tabstrip-tab-height) + var(--tabstrip-tab-spacing))';
    assertEquals(2, keyframes.length);
    assertEquals(
        `translate(calc(${horizontalScale} * ${
            horizontalTabSpacingVars}), calc(${verticalScale} * ${
            verticalTabSpacingVars}))`,
        keyframes[0]!['transform']);
    assertEquals('translate(0px, 0px)', keyframes[1]!['transform']);
  }

  /**
   * This function should be called once per test since the animations finishing
   * and being included in the getAnimations() calls can cause flaky tests.
   * @param direction, the direction the moved tab should animate.
   *     +1 if moving right, -1 if moving left
   */
  async function testPlaceTabElementAnimation(
      indexToMove: number, newIndex: number, direction: number) {
    await tabList.animationPromises;
    const unpinnedTabs = getUnpinnedTabs();

    const movedTab = unpinnedTabs[indexToMove]!;
    tabList.placeTabElement(movedTab, newIndex, false, null);
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
    await flushTasks();
    await tabList.animationPromises;

    // Test moving a tab within the same column. If a tab is moved from index 0
    // to index 2, it should move vertically down 2 places. Tabs at index 1 and
    // index 2 should move up 1 space.
    const pinnedTabs = getPinnedTabs();
    tabList.placeTabElement(pinnedTabs[0]!, 2, /*pinned=*/ true, null);
    await Promise.all([
      testPlaceElementAnimationParams(pinnedTabs[0]!, 0, -2),
      testPlaceElementAnimationParams(pinnedTabs[1]!, 0, 1),
      testPlaceElementAnimationParams(pinnedTabs[2]!, 0, 1),
    ]);
  });

  test(
      'PlacePinnedTabElementAnimatesTabsAcrossColumnsToHigherIndex',
      async () => {
        tabs.forEach(pinTabAt);
        for (let i = 0; i < 4; i++) {
          callbackRouter.tabCreated(createTab({
            id: tabs.length + i,
            index: tabs.length + i,
            pinned: true,
            title: 'Pinned tab',
          }));
        }
        await flushTasks();
        await tabList.animationPromises;

        const pinnedTabs = getPinnedTabs();
        tabList.placeTabElement(pinnedTabs[2]!, 6, /*pinned=*/ true, null);
        await Promise.all([
          testPlaceElementAnimationParams(pinnedTabs[2]!, -2, 2),
          testPlaceElementAnimationParams(pinnedTabs[3]!, 1, -2),
          testPlaceElementAnimationParams(pinnedTabs[4]!, 0, 1),
          testPlaceElementAnimationParams(pinnedTabs[5]!, 0, 1),
          testPlaceElementAnimationParams(pinnedTabs[6]!, 1, -2),
        ]);
      });

  test(
      'PlacePinnedTabElementAnimatesTabsAcrossColumnsToLowerIndex',
      async () => {
        tabs.forEach(pinTabAt);
        for (let i = 0; i < 4; i++) {
          callbackRouter.tabCreated(createTab({
            id: tabs.length + i,
            index: tabs.length + i,
            pinned: true,
            title: 'Pinned tab',
          }));
        }
        await flushTasks();
        await tabList.animationPromises;

        const pinnedTabs = getPinnedTabs();
        tabList.placeTabElement(pinnedTabs[3]!, 0, /*pinned=*/ true, null);
        await Promise.all([
          testPlaceElementAnimationParams(pinnedTabs[3]!, 1, 0),
          testPlaceElementAnimationParams(pinnedTabs[2]!, -1, 2),
          testPlaceElementAnimationParams(pinnedTabs[1]!, 0, -1),
          testPlaceElementAnimationParams(pinnedTabs[0]!, 0, -1),
        ]);
      });

  test('PlacesTabGroupElement', () => {
    const tabGroupElement = document.createElement('tabstrip-tab-group');
    tabList.placeTabGroupElement(tabGroupElement, 2);

    const tabGroupElements = getTabGroups();
    assertEquals(1, tabGroupElements.length);
    assertEquals(tabGroupElement, tabGroupElements[0]);

    // Group was inserted at index 2, so it should come after the 2nd tab.
    assertEquals(getUnpinnedTabs()[1], tabGroupElement.previousElementSibling);
  });

  async function testPlaceTabGroupElementAnimation(
      indexToGroup: number, newIndex: number, direction: number) {
    await tabList.animationPromises;

    // Group the tab at indexToGroup.
    const unpinnedTabs = getUnpinnedTabs();
    const tabToGroup = unpinnedTabs[indexToGroup]!;
    callbackRouter.tabGroupStateChanged(
        tabToGroup.tab.id, indexToGroup, 'group0');
    await flushTasks();

    const groupElement = tabToGroup.parentElement as TabGroupElement;
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
    const ungroupedTab = getUnpinnedTabs()[0]!;
    tabs.slice(1).forEach(tab => {
      callbackRouter.tabGroupStateChanged(tab.id, tab.index, 'group0');
    });
    await flushTasks();

    // Move the group to index 0.
    const tabGroup = getTabGroups()[0]!;
    tabList.placeTabGroupElement(tabGroup, 0);

    // Both the TabElement and TabGroupElement should move by a scale of 1.
    testPlaceElementAnimationParams(tabGroup, 1, 0);
    testPlaceElementAnimationParams(ungroupedTab, -1, 0);
  });

  test('AddNewTabGroup', async () => {
    const appendedTab = createTab({
      groupId: 'group0',
      id: 3,
      index: 3,
      title: 'New tab in group',
    });
    callbackRouter.tabCreated(appendedTab);
    await flushTasks();
    let tabElements = getUnpinnedTabs();
    assertEquals(tabs.length + 1, tabElements.length);
    assertEquals(getTabGroups().length, 1);
    assertEquals(
        'TABSTRIP-TAB-GROUP',
        tabElements[appendedTab.index]!.parentElement!.tagName);

    const prependedTab = createTab({
      groupId: 'group1',
      id: 4,
      index: 0,
      title: 'New tab',
    });
    callbackRouter.tabCreated(prependedTab);
    await flushTasks();
    tabElements = getUnpinnedTabs();
    assertEquals(tabs.length + 2, tabElements.length);
    assertEquals(getTabGroups().length, 2);
    assertEquals(
        'TABSTRIP-TAB-GROUP',
        tabElements[prependedTab.index]!.parentElement!.tagName);
  });

  test('AddTabToExistingGroup', async () => {
    const appendedTab = createTab({
      groupId: 'group0',
      id: 3,
      index: 3,
      title: 'New tab in group',
    });
    callbackRouter.tabCreated(appendedTab);
    await flushTasks();
    const appendedTabInSameGroup = createTab({
      groupId: 'group0',
      id: 4,
      index: 4,
      title: 'New tab in same group',
    });
    callbackRouter.tabCreated(appendedTabInSameGroup);
    await flushTasks();
    const tabGroups = getTabGroups();
    assertEquals(tabGroups.length, 1);
    const children = tabGroups[0]!.children as HTMLCollectionOf<TabElement>;
    assertEquals(children.item(0)!.tab.id, appendedTab.id);
    assertEquals(children.item(1)!.tab.id, appendedTabInSameGroup.id);
  });

  // Test that the TabList does not add a non-grouped tab to a tab group at the
  // same index.
  test('HandleSingleTabBeforeGroup', async () => {
    const tabInGroup = createTab({
      groupId: 'group0',
      id: 3,
      index: 3,
      title: 'New tab in group',
    });
    callbackRouter.tabCreated(tabInGroup);
    await flushTasks();
    const tabNotInGroup = createTab({
      id: 4,
      index: 3,
      title: 'New tab not in group',
    });
    callbackRouter.tabCreated(tabNotInGroup);
    await flushTasks();
    const tabsContainerChildren =
        tabList.shadowRoot!.querySelector('#unpinnedTabs')!.children;
    assertEquals(tabsContainerChildren.item(3)!.tagName, 'TABSTRIP-TAB');
    verifyTab((tabsContainerChildren.item(3) as TabElement).tab, tabNotInGroup);
    assertEquals(tabsContainerChildren.item(4)!.tagName, 'TABSTRIP-TAB-GROUP');
  });

  test('HandleGroupedTabBeforeDifferentGroup', async () => {
    const tabInOriginalGroup = tabs[1]!;
    callbackRouter.tabGroupStateChanged(
        tabInOriginalGroup.id, tabInOriginalGroup.index, 'originalGroup');

    // Create another group from the tab before group A.
    const tabInPrecedingGroup = tabs[0]!;
    callbackRouter.tabGroupStateChanged(
        tabInPrecedingGroup.id, tabInPrecedingGroup.index, 'precedingGroup');
    await flushTasks();
    const tabsContainerChildren =
        tabList.shadowRoot!.querySelector('#unpinnedTabs')!.children;

    const precedingGroup = tabsContainerChildren[0] as HTMLElement;
    assertEquals(precedingGroup.tagName, 'TABSTRIP-TAB-GROUP');
    assertEquals(precedingGroup.dataset['groupId'], 'precedingGroup');
    assertEquals(precedingGroup.children.length, 1);
    assertEquals(
        (precedingGroup.children[0] as TabElement).tab.id,
        tabInPrecedingGroup.id);

    const originalGroup = tabsContainerChildren[1] as HTMLElement;
    assertEquals(originalGroup.tagName, 'TABSTRIP-TAB-GROUP');
    assertEquals(originalGroup.dataset['groupId'], 'originalGroup');
    assertEquals(originalGroup.children.length, 1);
    assertEquals(
        (originalGroup.children[0] as TabElement).tab.id,
        tabInOriginalGroup.id);
  });

  test('HandleGroupedTabBeforeSameGroup', async () => {
    const originalTabInGroup = tabs[1]!;
    callbackRouter.tabGroupStateChanged(
        originalTabInGroup.id, originalTabInGroup.index, 'sameGroup');

    // Create another group from the tab before group A.
    const precedingTabInGroup = tabs[0]!;
    callbackRouter.tabGroupStateChanged(
        precedingTabInGroup.id, precedingTabInGroup.index, 'sameGroup');
    await flushTasks();

    const tabGroups = getTabGroups();
    const tabGroup = tabGroups[0]!;
    assertEquals(tabGroups.length, 1);
    assertEquals(tabGroup.dataset['groupId'], 'sameGroup');
    assertEquals(tabGroup.children.length, 2);
    assertEquals(
        (tabGroup.children[0] as TabElement).tab.id, precedingTabInGroup.id);
    assertEquals(
        (tabGroup.children[1] as TabElement).tab.id, originalTabInGroup.id);
  });

  test('removes a tab when tab is removed from current window', async () => {
    const tabToRemove = tabs[0]!;
    callbackRouter.tabRemoved(tabToRemove.id);
    await flushTasks();
    await tabList.animationPromises;
    assertEquals(tabs.length - 1, getUnpinnedTabs().length);
  });

  test('updates a tab with new tab data when a tab is updated', async () => {
    const tabToUpdate = tabs[0];
    const changeInfo = {title: 'A new title'};
    const updatedTab = Object.assign({}, tabToUpdate, changeInfo);
    callbackRouter.tabUpdated(updatedTab);
    await flushTasks();
    const tabElements = getUnpinnedTabs();
    verifyTab(tabElements[0]!.tab, updatedTab);
  });

  test('updates tabs when a new tab is activated', async () => {
    const tabElements = getUnpinnedTabs();

    // Mock activating the 2nd tab
    callbackRouter.tabActiveChanged(tabs[1]!.id);
    await flushTasks();
    assertFalse(tabElements[0]!.tab.active);
    assertTrue(tabElements[1]!.tab.active);
    assertFalse(tabElements[2]!.tab.active);
  });

  test('SingleActiveTabOnActiveTabCreated', async () => {
    let activeTabElements = getUnpinnedActiveTabs();
    assertEquals(activeTabElements.length, 1);

    const newActiveTab = createTab({
      active: true,
      id: tabs.length,
      index: tabs.length - 1,
      title: 'Tab 4',
    });
    callbackRouter.tabCreated(newActiveTab);
    await flushTasks();

    activeTabElements = getUnpinnedActiveTabs();
    assertEquals(activeTabElements.length, 1);
    assertEquals(activeTabElements[0]!.tab.id, newActiveTab.id);
  });

  test('adds a pinned tab to its designated container', async () => {
    callbackRouter.tabCreated(createTab({
      id: tabs.length,
      index: 0,
      title: 'New pinned tab',
      pinned: true,
    }));
    await flushTasks();
    const pinnedTabElements = getPinnedTabs();
    assertEquals(pinnedTabElements.length, 1);
    assertTrue(pinnedTabElements[0]!.tab.pinned);
  });

  test('moves pinned tabs to designated containers', async () => {
    const tabToPin = tabs[1]!;
    const changeInfo = {index: 0, pinned: true};
    let updatedTab = Object.assign({}, tabToPin, changeInfo);
    callbackRouter.tabUpdated(updatedTab);
    await flushTasks();

    const pinnedTabElements = getPinnedTabs();
    assertEquals(pinnedTabElements.length, 1);
    assertTrue(pinnedTabElements[0]!.tab.pinned);
    assertEquals(pinnedTabElements[0]!.tab.id, tabToPin.id);
    assertEquals(getUnpinnedTabs().length, 2);

    // Unpin the tab so that it's now at index 0
    changeInfo.index = 0;
    changeInfo.pinned = false;
    updatedTab = Object.assign({}, updatedTab, changeInfo);
    callbackRouter.tabUpdated(updatedTab);
    await flushTasks();

    const unpinnedTabElements = getUnpinnedTabs();
    assertEquals(getPinnedTabs().length, 0);
    assertEquals(unpinnedTabElements.length, 3);
    assertEquals(unpinnedTabElements[0]!.tab.id, tabToPin.id);
  });

  test('moves tab elements when tabs move', async () => {
    const tabElementsBeforeMove = getUnpinnedTabs();
    const tabToMove = tabs[0]!;
    callbackRouter.tabMoved(tabToMove.id, 2, false);
    await flushTasks();

    const tabElementsAfterMove = getUnpinnedTabs();
    assertEquals(tabElementsBeforeMove[0], tabElementsAfterMove[2]);
    assertEquals(tabElementsBeforeMove[1], tabElementsAfterMove[0]);
    assertEquals(tabElementsBeforeMove[2], tabElementsAfterMove[1]);
  });

  test('MoveExistingTabToGroup', async () => {
    const tabToGroup = tabs[1]!;
    callbackRouter.tabGroupStateChanged(
        tabToGroup.id, tabToGroup.index, 'group0');
    await flushTasks();
    let tabElements = getUnpinnedTabs();
    assertEquals(tabElements.length, tabs.length);
    assertEquals(
        tabElements[tabToGroup.index]!.parentElement!.tagName,
        'TABSTRIP-TAB-GROUP');

    const anotherTabToGroup = tabs[2]!;
    callbackRouter.tabGroupStateChanged(
        anotherTabToGroup.id, anotherTabToGroup.index, 'group0');
    await flushTasks();
    tabElements = getUnpinnedTabs();
    assertEquals(tabElements.length, tabs.length);
    assertEquals(
        tabElements[tabToGroup.index]!.parentElement,
        tabElements[anotherTabToGroup.index]!.parentElement);
  });

  test('MoveTabGroup', async () => {
    const tabToGroup = tabs[1]!;
    callbackRouter.tabGroupStateChanged(
        tabToGroup.id, tabToGroup.index, 'group0');
    callbackRouter.tabMoved(tabToGroup.id, 0, false);
    callbackRouter.tabGroupMoved('group0', 0);
    await flushTasks();

    const tabAtIndex0 = getUnpinnedTabs()[0]!;
    assertEquals(tabAtIndex0.parentElement!.tagName, 'TABSTRIP-TAB-GROUP');
    assertEquals(tabAtIndex0.tab.id, tabToGroup.id);
  });

  test('MoveTabGroupMultipleTabs', async () => {
    const tabToGroup1 = tabs[1]!;
    const tabToGroup2 = tabs[2]!;

    // Group tabs {1, 2} and assert the tab elements are correctly added under
    // their tab group element.
    callbackRouter.tabGroupStateChanged(
        tabToGroup1.id, tabToGroup1.index, 'group0');
    callbackRouter.tabGroupStateChanged(
        tabToGroup2.id, tabToGroup2.index, 'group0');
    await flushTasks();
    assertEquals(
        getUnpinnedTabs()[1]!.parentElement!.tagName, 'TABSTRIP-TAB-GROUP');
    assertEquals(
        getUnpinnedTabs()[2]!.parentElement!.tagName, 'TABSTRIP-TAB-GROUP');

    // During a drag and drop session that triggers a tab group move within the
    // WebUI tab strip the following sequence of events occur:
    //   1. The drag manager places the existing tab group element at the
    //      proposed drop index.
    //   2. The drag completes and the tab strip model is updated. This results
    //      in a series of tabMoved() events followed by a final tabGroupMoved()
    //      event.
    // The code below simulates this sequence of events and ensures the tab
    // strip responds correctly.

    // 1.
    const tabGroupElement = tabList.shadowRoot!.querySelector(
        'tabstrip-tab-group[data-group-id="group0"]')!;
    tabList.placeTabGroupElement(tabGroupElement as TabGroupElement, 0);

    // 2.
    callbackRouter.tabMoved(tabToGroup2.id, 0, false);
    callbackRouter.tabMoved(tabToGroup1.id, 0, false);
    callbackRouter.tabGroupMoved('group0', 0);
    await flushTasks();

    // Assert the tabs have moved as expected and are still members of their
    // oroginal tab group colloring the move.
    const tabAtIndex0 = getUnpinnedTabs()[0]!;
    assertEquals(tabAtIndex0.parentElement!.tagName, 'TABSTRIP-TAB-GROUP');
    assertEquals(tabAtIndex0.tab.id, tabToGroup1.id);

    const tabAtIndex1 = getUnpinnedTabs()[1]!;
    assertEquals(tabAtIndex1.parentElement!.tagName, 'TABSTRIP-TAB-GROUP');
    assertEquals(tabAtIndex1.tab.id, tabToGroup2.id);
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
    assertEquals(tabId, tabElements[2]!.tab.id);
    assertEquals(thumbnailTracked, false);
    assertEquals(testTabsApiProxy.getCallCount('setThumbnailTracked'), 1);
    testTabsApiProxy.reset();

    // Scroll such that the second tab is now the only visible tab. At this
    // point, all 3 tabs should fit within the root and rootMargin of the
    // IntersectionObserver. Since the 3rd tab was not being tracked before,
    // it should be the only tab to become tracked.
    tabList.scrollLeft = tabElements[1]!.offsetLeft;
    [tabId, thumbnailTracked] =
        await testTabsApiProxy.whenCalled('setThumbnailTracked');
    assertEquals(tabId, tabElements[2]!.tab.id);
    assertEquals(thumbnailTracked, true);
    assertEquals(testTabsApiProxy.getCallCount('setThumbnailTracked'), 1);
    testTabsApiProxy.reset();

    // Scroll such that the third tab is now the only visible tab. At this
    // point, the first tab should be outside of the rootMargin of the
    // IntersectionObserver.
    tabList.scrollLeft = tabElements[2]!.offsetLeft;
    [tabId, thumbnailTracked] =
        await testTabsApiProxy.whenCalled('setThumbnailTracked');
    assertEquals(tabId, tabElements[0]!.tab.id);
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
    tabElements[0]!.remove();
    await testTabsApiProxy.whenCalled('setThumbnailTracked');
    testTabsApiProxy.reset();
    tabElements[1]!.remove();
    await testTabsApiProxy.whenCalled('setThumbnailTracked');
    testTabsApiProxy.reset();

    // Pinning the third tab should untrack thumbnails for the tab
    pinTabAt(tabs[2]!, 0);
    let [tabId, thumbnailTracked] =
        await testTabsApiProxy.whenCalled('setThumbnailTracked');
    assertEquals(tabId, tabs[2]!.id);
    assertEquals(thumbnailTracked, false);
    testTabsApiProxy.reset();

    // Unpinning the tab should re-track the thumbnails
    unpinTabAt(tabs[2]!, 0);
    [tabId, thumbnailTracked] =
        await testTabsApiProxy.whenCalled('setThumbnailTracked');

    assertEquals(tabId, tabs[2]!.id);
    assertEquals(thumbnailTracked, true);
  });

  test('should update thumbnail track status on visibilitychange', async () => {
    await tabList.animationPromises;
    await testTabsApiProxy.whenCalled('setThumbnailTracked');
    testTabsApiProxy.reset();

    testTabsApiProxy.setVisible(false);
    document.dispatchEvent(new Event('visibilitychange'));

    // The tab strip should force untrack thumbnails for all tabs.
    await testTabsApiProxy.whenCalled('setThumbnailTracked');
    assertEquals(
        testTabsApiProxy.getCallCount('setThumbnailTracked'), tabs.length);
    testTabsApiProxy.reset();

    // Update width such that at all tabs can fit
    tabList.style.setProperty(
        '--tabstrip-tab-width', `${window.innerWidth / tabs.length}px`);

    testTabsApiProxy.setVisible(true);
    document.dispatchEvent(new Event('visibilitychange'));

    await testTabsApiProxy.whenCalled('setThumbnailTracked');
    assertEquals(
        testTabsApiProxy.getCallCount('setThumbnailTracked'), tabs.length);
  });

  // Flaky on all platforms. https://crbug.com/1247687.
  test.skip('ShouldDebounceThumbnailTrackerWhenScrollingFast', async () => {
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
      callbackRouter.tabCreated(createTab({
        id: tabs.length + i,
        index: tabs.length + i,
        title: `Tab ${tabs.length + i + 1}`,
      }));
    }
    await flushTasks();
    await tabList.animationPromises;
    await testTabsApiProxy.whenCalled('setThumbnailTracked');
    testTabsApiProxy.reset();
    testTabsApiProxy.resetThumbnailRequestCounts();

    // Mock 3 scroll events and end up with a scrolled state where the 10th
    // tab is aligned to the left. This should only evaluate to 1 set of
    // thumbnail updates and should most importantly skip the 6th tab.
    const tabElements = getUnpinnedTabs();
    tabList.scrollLeft = tabElements[3]!.offsetLeft;
    tabList.scrollLeft = tabElements[5]!.offsetLeft;
    tabList.scrollLeft = tabElements[10]!.offsetLeft;
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
      async () => {
        callbackRouter.receivedKeyboardFocus();
        await flushTasks();
        assertEquals(document.activeElement, tabList);
        assertEquals(tabList.shadowRoot!.activeElement, getUnpinnedTabs()[0]);
        assertTrue(FocusOutlineManager.forDocument(document).visible);
      });

  test('blurring the tab strip blurs the active element', async () => {
    // First, make sure tab strip has keyboard focus.
    callbackRouter.receivedKeyboardFocus();
    await flushTasks();

    window.dispatchEvent(new Event('blur'));
    assertEquals(tabList.shadowRoot!.activeElement, null);
  });

  test('should update the ID when a tab is replaced', async () => {
    assertEquals(getUnpinnedTabs()[0]!.tab.id, 0);
    callbackRouter.tabReplaced(tabs[0]!.id, 1000);
    await flushTasks();
    assertEquals(getUnpinnedTabs()[0]!.tab.id, 1000);
  });

  test('has custom context menu', async () => {
    const event =
        new PointerEvent('pointerup', {clientX: 1, clientY: 2, button: 2});
    document.dispatchEvent(event);

    const contextMenuArgs =
        await testTabsApiProxy.whenCalled('showBackgroundContextMenu');
    assertEquals(contextMenuArgs[0], 1);
    assertEquals(contextMenuArgs[1], 2);
  });

  test('scrolls to active tabs', async () => {
    await tabList.animationPromises;

    const scrollPadding = 32;
    const tabWidth = 200;
    const viewportWidth = 300;

    // Mock the width of each tab element.
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
    callbackRouter.tabActiveChanged(tabs[1]!.id);
    await flushTasks();
    testTabsApiProxy.setVisible(false);
    document.dispatchEvent(new Event('visibilitychange'));

    // The 2nd tab should be off-screen to the right, so activating it should
    // scroll so that the element's right edge is aligned with the screen's
    // right edge.
    const activeTab = getUnpinnedTabs()[1]!;
    assertEquals(
        tabList.scrollLeft + tabList.offsetWidth,
        activeTab.offsetLeft + activeTab.offsetWidth + scrollPadding);

    // The 1st tab should be now off-screen to the left, so activating it should
    // scroll so that the element's left edge is aligned with the screen's
    // left edge.
    callbackRouter.tabActiveChanged(tabs[0]!.id);
    await flushTasks();
    assertEquals(tabList.scrollLeft, 0);
  });

  test('PreventsDraggingWhenOnlyOneTab', () => {
    assertFalse(tabList.shouldPreventDrag(/*isDraggingTab=*/ true));
    const tabElements = getUnpinnedTabs();
    tabElements[1]!.remove();
    tabElements[2]!.remove();
    assertTrue(tabList.shouldPreventDrag(/*isDraggingTab=*/ true));
  });

  test('PreventsDraggingWhenOnlyOneTabGroup', async () => {
    // Create a tab group with 1 tab.
    const appendedTab = createTab({
      groupId: 'group0',
      id: 3,
      index: 3,
      title: 'New tab in group',
    });
    callbackRouter.tabCreated(appendedTab);
    await flushTasks();

    // Remove all tabs outside the tab group.
    const tabElements = getUnpinnedTabs();
    tabElements[0]!.remove();
    tabElements[1]!.remove();
    tabElements[2]!.remove();

    // At this point there's only 1 tab in 1 tab group.

    // Dragging a tab is not allowed.
    assertTrue(tabList.shouldPreventDrag(/*isDraggingTab=*/ true));

    // Dragging a tab group is not allowed.
    assertTrue(tabList.shouldPreventDrag(/*isDraggingTab=*/ false));

    // Add another tab in the same tab group.
    const appendedTabInSameGroup = createTab({
      groupId: 'group0',
      id: 4,
      index: 4,
      title: 'New tab in same group',
    });
    callbackRouter.tabCreated(appendedTabInSameGroup);
    await flushTasks();

    // At this point there are 2 tabs in 1 tab group;

    // Dragging a tab is still allowed.
    assertFalse(tabList.shouldPreventDrag(/*isDraggingTab=*/ true));

    // Dragging a tab group is not allowed.
    assertTrue(tabList.shouldPreventDrag(/*isDraggingTab=*/ false));
  });
});
