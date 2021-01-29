// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('net_internals_test', function() {
  /**
   * Returns the first tbody that's a descendant of |ancestorId|. If the
   * specified node is itself a table body node, just returns that node.
   * Returns null if no such node is found.
   * @param {string} ancestorId ID of an HTML element with a tbody descendant.
   * @return {node} The tbody node, or null.
   */
  function getTbodyDescendent(ancestorId) {
    if ($(ancestorId).nodeName === 'TBODY') {
      return $(ancestorId);
    }
    // The tbody element of the first styled table in |parentId|.
    return document.querySelector('#' + ancestorId + ' tbody');
  }

  /**
   * Finds the first tbody that's a descendant of |ancestorId|, including the
   * |ancestorId| element itself, and returns the number of rows it has.
   * Returns -1 if there's no such table.  Excludes hidden rows.
   * @param {string} ancestorId ID of an HTML element with a tbody descendant.
   * @return {number} Number of rows the style table's body has.
   */
  function getTbodyNumRows(ancestorId) {
    // The tbody element of the first styled table in |parentId|.
    var tbody = getTbodyDescendent(ancestorId);
    if (!tbody) {
      return -1;
    }
    var visibleChildren = 0;
    for (var i = 0; i < tbody.children.length; ++i) {
      if (nodeIsVisible(tbody.children[i])) {
        ++visibleChildren;
      }
    }
    return visibleChildren;
  }

  /**
   * Finds the first tbody that's a descendant of |ancestorId|, including the
   * |ancestorId| element itself, and checks if it has exactly |expectedRows|
   * rows.  Does not count hidden rows.
   * @param {string} ancestorId ID of an HTML element with a tbody descendant.
   * @param {number} expectedRows Expected number of rows in the table.
   */
  function checkTbodyRows(ancestorId, expectedRows) {
    expectEquals(
        expectedRows, getTbodyNumRows(ancestorId),
        'Incorrect number of rows in ' + ancestorId);
  }

  /**
   * Finds the tbody that's a descendant of |ancestorId|, including the
   * |ancestorId| element itself, and returns the text of the specified cell.
   * If the cell does not exist, throws an exception.  Skips over hidden rows.
   * @param {string} ancestorId ID of an HTML element with a tbody descendant.
   * @param {number} row Row of the value to retrieve.
   * @param {number} column Column of the value to retrieve.
   */
  function getTbodyText(ancestorId, row, column) {
    var tbody = getTbodyDescendent(ancestorId);
    var currentChild = tbody.children[0];
    while (currentChild) {
      if (nodeIsVisible(currentChild)) {
        if (row === 0) {
          return currentChild.children[column].innerText;
        }
        --row;
      }
      currentChild = currentChild.nextElementSibling;
    }
    return 'invalid row';
  }

  /**
   * Returns the view and menu item node for the tab with given id.
   * Asserts if the tab can't be found.
   * @param {string}: tabId Id of the tab to lookup.
   * @return {Object}
   */
  function getTab(tabId) {
    var tabSwitcher = MainView.getInstance().tabSwitcher();
    var view = tabSwitcher.getTabView(tabId);
    var tabLink = tabSwitcher.tabIdToLink_[tabId];

    assertNotEquals(view, undefined, tabId + ' does not exist.');
    assertNotEquals(tabLink, undefined, tabId + ' does not exist.');

    return {
      view: view,
      tabLink: tabLink,
    };
  }

  /**
   * Returns true if the node is visible.
   * @param {Node}: node Node to check the visibility state of.
   * @return {bool} Whether or not the node is visible.
   */
  function nodeIsVisible(node) {
    return node.style.display !== 'none';
  }

  /**
   * Returns true if the specified tab's link is visible, false otherwise.
   * Asserts if the link can't be found.
   * @param {string}: tabId Id of the tab to check.
   * @return {bool} Whether or not the tab's link is visible.
   */
  function tabLinkIsVisible(tabId) {
    var tabLink = getTab(tabId).tabLink;
    return nodeIsVisible(tabLink);
  }

  /**
   * Returns the id of the currently active tab.
   * @return {string} ID of the active tab.
   */
  function getActiveTabId() {
    return MainView.getInstance().tabSwitcher().getActiveTabId();
  }

  /**
   * Returns the tab id of a tab, given its associated URL hash value.  Asserts
   *     if |hash| has no associated tab.
   * @param {string}: hash Hash associated with the tab to return the id of.
   * @return {string} String identifier of the tab with the given hash.
   */
  function getTabId(hash) {
    /**
     * Map of tab ids to location hashes.  Since the text fixture must be
     * runnable independent of net-internals, for generating the test's cc
     * files, must be careful to only create this map while a test is running.
     * @type {object.<string, string>}
     */
    var hashToTabIdMap = {
      events: EventsView.TAB_ID,
      proxy: ProxyView.TAB_ID,
      dns: DnsView.TAB_ID,
      sockets: SocketsView.TAB_ID,
      hsts: DomainSecurityPolicyView.TAB_ID,
      chromeos: CrosView.TAB_ID
    };

    assertEquals(
        typeof hashToTabIdMap[hash], 'string', 'Invalid tab anchor: ' + hash);
    var tabId = hashToTabIdMap[hash];
    assertEquals('object', typeof getTab(tabId), 'Invalid tab: ' + tabId);
    return tabId;
  }

  /**
   * Switches to the specified tab.
   * @param {string}: hash Hash associated with the tab to switch to.
   */
  function switchToView(hash) {
    var tabId = getTabId(hash);

    // Make sure the tab link is visible, as we only simulate normal usage.
    expectTrue(
        tabLinkIsVisible(tabId), tabId + ' does not have a visible tab link.');
    var tabLinkNode = getTab(tabId).tabLink;

    // Simulate a left click on the link.
    var mouseEvent = document.createEvent('MouseEvents');
    mouseEvent.initMouseEvent(
        'click', true, true, window, 1, 0, 0, 0, 0, false, false, false, false,
        0, null);
    tabLinkNode.dispatchEvent(mouseEvent);

    // Make sure the hash changed.
    assertEquals('#' + hash, document.location.hash);

    // Run the onhashchange function, so can test the resulting state.
    // Otherwise, the method won't trigger synchronously.
    window.onhashchange();

    // Make sure only the specified tab is visible.
    var tabSwitcher = MainView.getInstance().tabSwitcher();
    var tabIdToView = tabSwitcher.getAllTabViews();
    for (var curTabId in tabIdToView) {
      expectEquals(
          curTabId === tabId, tabSwitcher.getTabView(curTabId).isVisible(),
          curTabId + ': Unexpected visibility state.');
    }
  }

  /**
   * Checks the visibility of all tab links against expected values.
   * @param {object.<string, bool>}: tabVisibilityState Object with a an entry
   *     for each tab's hash, and a bool indicating if it should be visible or
   *     not.
   * @param {bool+}: tourTabs True if tabs expected to be visible should should
   *     each be navigated to as well.
   */
  function checkTabLinkVisibility(tabVisibilityState, tourTabs) {
    // The currently active tab should have a link that is visible.
    expectTrue(tabLinkIsVisible(getActiveTabId()));

    // Check visibility state of all tabs.
    var tabCount = 0;
    for (var hash in tabVisibilityState) {
      var tabId = getTabId(hash);
      assertEquals('object', typeof getTab(tabId), 'Invalid tab: ' + tabId);
      expectEquals(
          tabVisibilityState[hash], tabLinkIsVisible(tabId),
          tabId + ' visibility state is unexpected.');
      if (tourTabs && tabVisibilityState[hash]) {
        switchToView(hash);
      }
      tabCount++;
    }

    // Check that every tab was listed.
    var tabSwitcher = MainView.getInstance().tabSwitcher();
    var tabIdToView = tabSwitcher.getAllTabViews();
    var expectedTabCount = 0;
    for (tabId in tabIdToView) {
      expectedTabCount++;
    }
    expectEquals(tabCount, expectedTabCount);
  }

  /**
   * Returns true if a node does not have a 'display' property of 'none'.
   * @param {node}: node The node to check.
   */
  function isDisplayed(node) {
    var style = getComputedStyle(node);
    return style.getPropertyValue('display') !== 'none';
  }

  /**
   * Checks that only the given status view node is visible.
   * @param {string}: nodeId ID of the node that should be visible.
   */
  function expectStatusViewNodeVisible(nodeId) {
    var allIds = [
      CaptureStatusView.MAIN_BOX_ID, LoadedStatusView.MAIN_BOX_ID,
      HaltedStatusView.MAIN_BOX_ID
    ];

    for (var i = 0; i < allIds.length; ++i) {
      var curId = allIds[i];
      expectEquals(nodeId === curId, nodeIsVisible($(curId)));
    }
  }

  return {
    expectStatusViewNodeVisible: expectStatusViewNodeVisible,
    isDisplayed: isDisplayed,
    checkTbodyRows: checkTbodyRows,
    getTbodyDescendent: getTbodyDescendent,
    getTbodyNumRows: getTbodyNumRows,
    getTbodyText: getTbodyText,
    nodeIsVisible: nodeIsVisible,
    tabLinkIsVisible: tabLinkIsVisible,
    getTab: getTab,
    getActiveTabId: getActiveTabId,
    getTabId: getTabId,
    switchToView: switchToView,
    checkTabLinkVisibility: checkTabLinkVisibility,
  };
});
