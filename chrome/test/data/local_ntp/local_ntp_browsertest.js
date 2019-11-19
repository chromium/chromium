// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests the local NTP.
 */

/**
 * Enum for HTML element ids.
 * @enum {string}
 * @const
 */
const IDS = {
  // Error notification ids.
  ERROR: 'error-notice',
  ERROR_CONTAINER: 'error-notice-container',
  // Promo id.
  PROMO: 'promo',
  // Success notification ids.
  SUCCESS: 'mv-notice',
  SUCCESS_CONTAINER: 'mv-notice-container',
};

/**
 * Local NTP's object for test and setup functions.
 */
test.localNtp = {};

/**
 * Sets up the page for each individual test.
 */
test.localNtp.setUp = function() {
  setUpPage('local-ntp-template');
};

// ******************************* SIMPLE TESTS *******************************
// These are run by runSimpleTests above.
// Functions from test_utils.js are automatically imported.

/**
 * Tests that Google NTPs show a fakebox and logo.
 */
test.localNtp.testShowsFakeboxAndLogoIfGoogle = function() {
  initLocalNTP(/*isGooglePage=*/true);
  assertTrue(elementIsVisible($('fakebox')));
  assertTrue(elementIsVisible($('logo')));
};

/**
 * Tests that non-Google NTPs do not show a fakebox.
 */
test.localNtp.testDoesNotShowFakeboxIfNotGoogle = function() {
  initLocalNTP(/*isGooglePage=*/false);
  assertFalse(elementIsVisible($('fakebox')));
  assertFalse(elementIsVisible($('logo')));
};

/**
 * Tests that the embeddedSearch.newTabPage.mostVisited API is
 * hooked up, and provides the correct data for the tiles (i.e. only
 * IDs, no URLs).
 */
test.localNtp.testMostVisitedContents = function() {
  // Check that the API is available and properly hooked up, so that it returns
  // some data (see history::PrepopulatedPageList for the default contents).
  assertTrue(window.chrome.embeddedSearch.newTabPage.mostVisited.length > 0);

  // Check that the items have the required fields: We expect a "restricted ID"
  // (rid), but there mustn't be url, title, etc. Those are only available
  // through getMostVisitedItemData(rid).
  for (var mvItem of window.chrome.embeddedSearch.newTabPage.mostVisited) {
    assertTrue(isFinite(mvItem.rid));
    assertTrue(!mvItem.url);
    assertTrue(!mvItem.title);
    assertTrue(!mvItem.domain);
  }

  // Try to get an item's details via getMostVisitedItemData. This should fail,
  // because that API is only available to the MV iframe.
  assertTrue(!window.chrome.embeddedSearch.newTabPage.getMostVisitedItemData(
      window.chrome.embeddedSearch.newTabPage.mostVisited[0].rid));
};

/**
 * Tests that the custom link notifications for success and error are shown.
 */
test.localNtp.testCustomLinkNotifications = function() {
  let localNTP = initLocalNTP(/*isGooglePage=*/ true);

  // Override timeout creation.
  window.setTimeout = (timeout, duration) => {
    timeout();
  };
  let delayedHide = () => {};
  localNTP.overrideExecutableTimeoutForTesting((timeout, duration) => {
    delayedHide = timeout;
  });

  assertNoNotificationVisible();

  // Test that success notifications are shown and properly hidden.

  window.chrome.embeddedSearch.newTabPage.onaddcustomlinkdone(true);
  assertSuccessNotificationVisible();
  // Simulate the notification timing out. This should hide it.
  delayedHide(/*executedEarly=*/ false);
  // Notification visibility is not set until the transition ends.
  fireTransitionEnd($(IDS.SUCCESS_CONTAINER), 'bottom');
  assertNoNotificationVisible();

  window.chrome.embeddedSearch.newTabPage.onupdatecustomlinkdone(true);
  assertSuccessNotificationVisible();
  delayedHide(/*executedEarly=*/ false);
  fireTransitionEnd($(IDS.SUCCESS_CONTAINER), 'bottom');
  assertNoNotificationVisible();

  window.chrome.embeddedSearch.newTabPage.ondeletecustomlinkdone(true);
  assertSuccessNotificationVisible();
  delayedHide(/*executedEarly=*/ false);
  fireTransitionEnd($(IDS.SUCCESS_CONTAINER), 'bottom');
  assertNoNotificationVisible();

  // Test that error notifications are shown and properly hidden.

  window.chrome.embeddedSearch.newTabPage.onaddcustomlinkdone(false);
  assertErrorNotificationVisible();
  delayedHide(/*executedEarly=*/ false);
  fireTransitionEnd($(IDS.ERROR_CONTAINER), 'bottom');
  assertNoNotificationVisible();

  window.chrome.embeddedSearch.newTabPage.onupdatecustomlinkdone(false);
  assertErrorNotificationVisible();
  delayedHide(/*executedEarly=*/ false);
  fireTransitionEnd($(IDS.ERROR_CONTAINER), 'bottom');
  assertNoNotificationVisible();

  window.chrome.embeddedSearch.newTabPage.ondeletecustomlinkdone(false);
  assertErrorNotificationVisible();
  delayedHide(/*executedEarly=*/ false);
  fireTransitionEnd($(IDS.ERROR_CONTAINER), 'bottom');
  assertNoNotificationVisible();
};

/**
 * Tests that notifications for success and error are shown properly when a
 * promo is visible (i.e. the promo should not be visible when a notification is
 * present).
 */
test.localNtp.testNotificationsWithPromo = function() {
  let localNTP = initLocalNTP(/*isGooglePage=*/ true);

  addTestPromo();

  // Override timeout creation.
  window.setTimeout = (timeout, duration) => {
    timeout();
  };
  let delayedHide = () => {};
  localNTP.overrideExecutableTimeoutForTesting((timeout, duration) => {
    delayedHide = timeout;
  });

  assertNoNotificationVisible(/*hasPromo=*/ true);

  // Show success notification.
  window.chrome.embeddedSearch.newTabPage.onaddcustomlinkdone(true);
  // Promo visibility is not set until the transition ends.
  fireTransitionEnd($(IDS.PROMO), 'bottom');
  assertSuccessNotificationVisible(/*hasPromo=*/ true);
  // Simulate the notification timing out. This should hide it and re-show the
  // promo.
  delayedHide(/*executedEarly=*/ false);
  fireTransitionEnd($(IDS.SUCCESS_CONTAINER), 'bottom');
  assertNoNotificationVisible(/*hasPromo=*/ true);

  // Show error notification.
  window.chrome.embeddedSearch.newTabPage.onaddcustomlinkdone(false);
  fireTransitionEnd($(IDS.PROMO), 'bottom');
  assertErrorNotificationVisible(/*hasPromo=*/ true);
  delayedHide(/*executedEarly=*/ false);
  fireTransitionEnd($(IDS.ERROR_CONTAINER), 'bottom');
  assertNoNotificationVisible(/*hasPromo=*/ true);
};

/**
 * Tests that different notification types will be displayed properly if they
 * are shown consecutively (i.e. only one notification/promo should be visible
 * at a time).
 */
test.localNtp.testMultipleNotificationsShown = function() {
  let localNTP = initLocalNTP(/*isGooglePage=*/ true);

  // Override timeout creation.
  window.setTimeout = (timeout, duration) => {
    timeout();
  };
  let delayedHide = () => {};
  let triggered = false;
  localNTP.overrideExecutableTimeoutForTesting((timeout, duration) => {
    delayedHide = timeout;
    return {
      clear: () => {},
      trigger: () => {
        triggered = true;  // Save whether this timeout was executed early.
        return timeout(true);
      }
    };
  });

  assertNoNotificationVisible();

  // While a promo is not visible.

  // Perform two successful actions consecutively. The success notification
  // should stay visible for both.
  window.chrome.embeddedSearch.newTabPage.onupdatecustomlinkdone(true);
  // Early execution of the delayed timeout should not have occurred.
  assertFalse(triggered);
  assertSuccessNotificationVisible();
  window.chrome.embeddedSearch.newTabPage.ondeletecustomlinkdone(true);
  assertFalse(triggered);
  assertSuccessNotificationVisible();

  // Perform an unsuccessful action while the success notification is visible.
  // The error notification should replace the success notification.
  window.chrome.embeddedSearch.newTabPage.onupdatecustomlinkdone(false);
  fireTransitionEnd($(IDS.SUCCESS_CONTAINER), 'bottom');
  // The delayed timeout for the previous notification should have been fired.
  assertTrue(triggered);
  assertErrorNotificationVisible();

  // Simulate the current notification timing out. This should hide it.
  delayedHide(false);
  fireTransitionEnd($(IDS.ERROR_CONTAINER), 'bottom');
  assertNoNotificationVisible();

  // While a promo is visible.

  addTestPromo();

  assertTrue(elementIsVisible($(IDS.PROMO)));
  triggered = false;

  // With promo
  window.chrome.embeddedSearch.newTabPage.onupdatecustomlinkdone(true);
  fireTransitionEnd($(IDS.PROMO), 'bottom');
  assertFalse(triggered);
  assertSuccessNotificationVisible(/*hasPromo=*/ true);
  window.chrome.embeddedSearch.newTabPage.ondeletecustomlinkdone(true);
  assertFalse(triggered);
  assertSuccessNotificationVisible(/*hasPromo=*/ true);
  window.chrome.embeddedSearch.newTabPage.onupdatecustomlinkdone(false);
  fireTransitionEnd($(IDS.SUCCESS_CONTAINER), 'bottom');
  assertTrue(triggered);
  assertErrorNotificationVisible(/*hasPromo=*/ true);
  // Hide the notification and indicate that this was not an early execution.
  // The promo should become visible.
  delayedHide(false);
  fireTransitionEnd($(IDS.ERROR_CONTAINER), 'bottom');
  assertNoNotificationVisible(/*hasPromo=*/ true);
};

// ***************************** HELPER FUNCTIONS *****************************
// Helper functions used in tests.

/**
 * Add a test promo to the page.
 */
function addTestPromo() {
  let promo = document.createElement('div');
  promo.id = IDS.PROMO;
  promo.innerHTML = 'test';
  promo.style.visibility = 'visible';
  $('ntp-contents').appendChild(promo);
}

/**
 * Fires a "transitionend" event on the element.
 * @param {!Element} element The element on which to fire a transitionEnd event.
 * @param {string} propertyName The property name of the event.
 */
function fireTransitionEnd(element, propertyName) {
  const event = new Event('transitionend');
  event.propertyName = propertyName;
  element.dispatchEvent(event);
}

/**
 * Check that no notification is visible. If a promo exists, check that the
 * promo is visible.
 * @param {?boolean=} hasPromo True if a promo is on the page.
 */
function assertNoNotificationVisible(hasPromo = false) {
  assertFalse(
      elementIsVisible($(IDS.SUCCESS)), 'Success notification is visible');
  assertFalse(elementIsVisible($(IDS.ERROR)), 'Error notification is visible');
  if (hasPromo) {
    assertTrue(elementIsVisible($(IDS.PROMO)), 'Promo is not visible');
  }
}

/**
 * Check that the success notification is visible. If a promo exists, check that
 * the promo is not visible.
 * @param {?boolean=} hasPromo True if a promo is on the page.
 */
function assertSuccessNotificationVisible(hasPromo = false) {
  assertTrue(
      elementIsVisible($(IDS.SUCCESS)), 'Success notification is not visible');
  assertFalse(elementIsVisible($(IDS.ERROR)), 'Error notification is visible');
  if (hasPromo) {
    assertFalse(elementIsVisible($(IDS.PROMO)), 'Promo is visible');
  }
}

/**
 * Check that the error notification is visible. If a promo exists, check that
 * the promo is not visible.
 * @param {?boolean=} hasPromo True if a promo is on the page.
 */
function assertErrorNotificationVisible(hasPromo = false) {
  assertFalse(
      elementIsVisible($(IDS.SUCCESS)), 'Success notification is visible');
  assertTrue(
      elementIsVisible($(IDS.ERROR)), 'Error notification is not visible');
  if (hasPromo) {
    assertFalse(elementIsVisible($(IDS.PROMO)), 'Promo is visible');
  }
}
