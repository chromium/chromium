// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationOrigin, PrintPreviewLinkContainerElement} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.js';
import {isWindows} from 'chrome://resources/js/cr.m.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {getCddTemplate} from './print_preview_test_utils.js';

const link_container_test = {
  suiteName: 'LinkContainerTest',
  TestNames: {
    HideInAppKioskMode: 'hide in app kiosk mode',
    SystemDialogLinkClick: 'system dialog link click',
    InvalidState: 'invalid state',
    OpenInPreviewLinkClick: 'open in preview link click',
  },
};

Object.assign(window, {link_container_test: link_container_test});

suite(link_container_test.suiteName, function() {
  let linkContainer: PrintPreviewLinkContainerElement;

  setup(function() {
    document.body.innerHTML =
        window.trustedTypes!.emptyHTML as unknown as string;
    linkContainer = document.createElement('print-preview-link-container');
    document.body.appendChild(linkContainer);

    /** Set inputs to some defaults. */
    const fooDestination =
        new Destination('FooPrinter', DestinationOrigin.LOCAL, 'Foo Printer');
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

    const link = linkContainer.$.systemDialogLink;
    link.click();
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

    function validateLinkState(link: HTMLDivElement, disabled: boolean) {
      assertFalse(link.hidden);
      assertEquals(!disabled, link.hasAttribute('actionable'));
      assertEquals(disabled, link.querySelector('cr-icon-button')!.disabled);
    }

    validateLinkState(systemDialogLink, false);
    // <if expr="is_macosx">
    const openInPreviewLink = linkContainer.$.openPdfInPreviewLink;
    validateLinkState(openInPreviewLink, false);
    // </if>

    // Set disabled to true, indicating that there is a validation error or
    // printer error.
    linkContainer.disabled = true;
    validateLinkState(systemDialogLink, isWindows);
    // <if expr="is_macosx">
    validateLinkState(assert(openInPreviewLink), true);
    // </if>
  });

  // <if expr="is_macosx">
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
  // </if>
});
