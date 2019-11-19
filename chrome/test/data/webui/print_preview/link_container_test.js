// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {isMac, isWindows} from 'chrome://resources/js/cr.m.js';
import {getCddTemplate} from 'chrome://test/print_preview/print_preview_test_utils.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

window.link_container_test = {};
link_container_test.suiteName = 'LinkContainerTest';
/** @enum {string} */
link_container_test.TestNames = {
  HideInAppKioskMode: 'hide in app kiosk mode',
  SystemDialogLinkClick: 'system dialog link click',
  InvalidState: 'invalid state',
  OpenInPreviewLinkClick: 'open in preview link click',
};

suite(link_container_test.suiteName, function() {
  /** @type {?PrintPreviewLinkContainerElement} */
  let linkContainer = null;

  /** @override */
  setup(function() {
    PolymerTest.clearBody();
    linkContainer = document.createElement('print-preview-link-container');
    document.body.appendChild(linkContainer);

    /** Set inputs to some defaults. */
    const fooDestination = new Destination(
        'FooPrinter', DestinationType.LOCAL, DestinationOrigin.LOCAL,
        'Foo Printer', DestinationConnectionStatus.ONLINE);
    fooDestination.capabilities =
        getCddTemplate(fooDestination.id).capabilities;
    linkContainer.destination = fooDestination;
    linkContainer.appKioskMode = false;
    linkContainer.disabled = false;
  });

  /** Tests that the system dialog link is hidden in App Kiosk mode. */
  test(assert(link_container_test.TestNames.HideInAppKioskMode), function() {
    const systemDialogLink = linkContainer.$.systemDialogLink;
    assertFalse(systemDialogLink.hidden);
    linkContainer.set('appKioskMode', true);
    assertTrue(systemDialogLink.hidden);
  });

  /**
   * Test that clicking the system dialog link click results in an event
   * firing, and the throbber appears on non-Windows.
   */
  test(assert(link_container_test.TestNames.SystemDialogLinkClick), function() {
    const promise = eventToPromise('print-with-system-dialog', linkContainer);
    const throbber = linkContainer.$.systemDialogThrobber;
    assertTrue(throbber.hidden);

    linkContainer.$.systemDialogLink.click();
    return promise.then(function() {
      assertEquals(isWindows, throbber.hidden);
    });
  });

  /**
   * Test that if settings are invalid, the open in preview link is disabled
   * (if it exists), and that the system dialog link is disabled on Windows
   * and enabled on other platforms.
   */
  test(assert(link_container_test.TestNames.InvalidState), function() {
    const systemDialogLink = linkContainer.$.systemDialogLink;
    const openInPreviewLink =
        isMac ? linkContainer.$.openPdfInPreviewLink : null;

    const validateLinkState = (link, disabled) => {
      assertFalse(link.hidden);
      assertEquals(!disabled, link.hasAttribute('actionable'));
      assertEquals(disabled, link.querySelector('cr-icon-button').disabled);
    };

    validateLinkState(systemDialogLink, false);
    if (isMac) {
      validateLinkState(openInPreviewLink, false);
    }

    // Set disabled to true, indicating that there is a validation error or
    // printer error.
    linkContainer.disabled = true;
    validateLinkState(systemDialogLink, isWindows);
    if (isMac) {
      validateLinkState(openInPreviewLink, true);
    }
  });

  /**
   * Test that clicking the open in preview link correctly results in a
   * property change and that the throbber appears. Mac only.
   */
  test(
      assert(link_container_test.TestNames.OpenInPreviewLinkClick), function() {
        const throbber = linkContainer.$.openPdfInPreviewThrobber;
        assertTrue(throbber.hidden);
        const promise = eventToPromise('open-pdf-in-preview', linkContainer);

        linkContainer.$.openPdfInPreviewLink.click();
        return promise.then(function() {
          assertFalse(throbber.hidden);
        });
      });
});
