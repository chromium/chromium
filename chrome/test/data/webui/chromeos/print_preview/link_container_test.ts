// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewLinkContainerElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin} from 'chrome://print/print_preview.js';
// <if expr="is_macosx">
import {assert} from 'chrome://resources/js/assert.js';
// </if>
import {isWindows} from 'chrome://resources/js/platform.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {getCddTemplate} from './print_preview_test_utils.js';

function assertLinkState(link: HTMLElement, disabled: boolean) {
  assertFalse(link.hidden);
  assertEquals(!disabled, link.hasAttribute('actionable'));
  assertEquals(disabled, link.querySelector('cr-icon-button')!.disabled);
}

suite('LinkContainerTest', function() {
  let linkContainer: PrintPreviewLinkContainerElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
  test('HideInAppKioskMode', function() {
    const systemDialogLink = linkContainer.$.systemDialogLink;
    assertFalse(systemDialogLink.hidden);
    linkContainer.set('appKioskMode', true);
    assertTrue(systemDialogLink.hidden);
  });

  /**
   * Test that clicking the system dialog link click results in an event
   * firing, and the throbber appears on non-Windows.
   */
  test('SystemDialogLinkClick', function() {
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
   * Test that the system dialog link properties are as expected.
   */
  test('SystemDialogLinkProperties', function() {
    const link = linkContainer.$.systemDialogLink;
    assertLinkState(link, false);

    // <if expr="is_macosx">
    assertEquals('Print using system dialog… (⌥⌘P)', link.textContent);
    // </if>
    // <if expr="not is_macosx">
    assertEquals('Print using system dialog… (Ctrl+Shift+P)', link.textContent);
    // </if>
  });

  /**
   * Test that if settings are invalid, the open in preview link is disabled
   * (if it exists), and that the system dialog link is disabled on Windows
   * and enabled on other platforms.
   */
  test('InvalidState', function() {
    const systemDialogLink = linkContainer.$.systemDialogLink;

    assertLinkState(systemDialogLink, false);
    // <if expr="is_macosx">
    const openInPreviewLink = linkContainer.$.openPdfInPreviewLink;
    assertLinkState(openInPreviewLink, false);
    // </if>

    // Set disabled to true, indicating that there is a validation error or
    // printer error.
    linkContainer.disabled = true;
    assertLinkState(systemDialogLink, isWindows);
    // <if expr="is_macosx">
    assert(openInPreviewLink);
    assertLinkState(openInPreviewLink, true);
    // </if>
  });

  // <if expr="is_macosx">
  /**
   * Test that clicking the open in preview link correctly results in a
   * property change and that the throbber appears. Mac only.
   */
  test(
      'OpenInPreviewLinkClick', function() {
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
