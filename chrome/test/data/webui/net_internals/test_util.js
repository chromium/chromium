// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="chromeos_ash">
import {CrosView} from 'chrome://net-internals/chromeos_view.js';
// </if>
import {DnsView} from 'chrome://net-internals/dns_view.js';
import {DomainSecurityPolicyView} from 'chrome://net-internals/domain_security_policy_view.js';
import {EventsView} from 'chrome://net-internals/events_view.js';
import {MainView} from 'chrome://net-internals/main.js';
import {ProxyView} from 'chrome://net-internals/proxy_view.js';
import {SharedDictionaryView} from 'chrome://net-internals/shared_dictionary_view.js';
import {SocketsView} from 'chrome://net-internals/sockets_view.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

/**
 * Returns the view and menu item node for the tab with given id.
 * Asserts if the tab can't be found.
 * @param {string}: tabId Id of the tab to lookup.
 * @return {Object}
 */
function getTab(tabId) {
  const tabSwitcher = MainView.getInstance().tabSwitcher();
  const view = tabSwitcher.getTabView(tabId);
  const tabLink = tabSwitcher.tabIdToLink_[tabId];

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
    const tabLink = getTab(tabId).tabLink;
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
    const hashToTabIdMap = {
      events: EventsView.TAB_ID,
      proxy: ProxyView.TAB_ID,
      dns: DnsView.TAB_ID,
      sockets: SocketsView.TAB_ID,
      hsts: DomainSecurityPolicyView.TAB_ID,
      sharedDictionary: SharedDictionaryView.TAB_ID,
      // <if expr="chromeos_ash">
      chromeos: CrosView.TAB_ID,
      // </if>
    };

    assertEquals(
        typeof hashToTabIdMap[hash], 'string', 'Invalid tab anchor: ' + hash);
    const tabId = hashToTabIdMap[hash];
    assertEquals('object', typeof getTab(tabId), 'Invalid tab: ' + tabId);
    return tabId;
  }

  /**
   * Switches to the specified tab.
   * @param {string}: hash Hash associated with the tab to switch to.
   */
  export function switchToView(hash) {
    const tabId = getTabId(hash);

    // Make sure the tab link is visible, as we only simulate normal usage.
    assertTrue(
        tabLinkIsVisible(tabId), tabId + ' does not have a visible tab link.');
    const tabLinkNode = getTab(tabId).tabLink;

    // Simulate a left click on the link.
    const mouseEvent = document.createEvent('MouseEvents');
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
    const tabSwitcher = MainView.getInstance().tabSwitcher();
    const tabIdToView = tabSwitcher.getAllTabViews();
    for (const curTabId in tabIdToView) {
      assertEquals(
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
  export function checkTabLinkVisibility(tabVisibilityState, tourTabs) {
    // The currently active tab should have a link that is visible.
    assertTrue(tabLinkIsVisible(getActiveTabId()));

    // Check visibility state of all tabs.
    let tabCount = 0;
    for (const hash in tabVisibilityState) {
      const tabId = getTabId(hash);
      assertEquals('object', typeof getTab(tabId), 'Invalid tab: ' + tabId);
      assertEquals(
          tabVisibilityState[hash], tabLinkIsVisible(tabId),
          tabId + ' visibility state is unexpected.');
      if (tourTabs && tabVisibilityState[hash]) {
        switchToView(hash);
      }
      tabCount++;
    }

    // Check that every tab was listed.
    const tabSwitcher = MainView.getInstance().tabSwitcher();
    const tabIdToView = tabSwitcher.getAllTabViews();
    let expectedTabCount = 0;
    for (const tabId in tabIdToView) {
      expectedTabCount++;
    }
    assertEquals(tabCount, expectedTabCount);
  }
