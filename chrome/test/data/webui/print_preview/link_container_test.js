// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('link_container_test', function() {
  /** @enum {string} */
  const TestNames = {
    HideInAppKioskMode: 'hide in app kiosk mode',
    SystemDialogLinkClick: 'system dialog link click',
    InvalidState: 'invalid state',
    OpenInPreviewLinkClick: 'open in preview link click',
  };

  const suiteName = 'LinkContainerTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewLinkContainerElement} */
    let linkContainer = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      linkContainer = document.createElement('print-preview-link-container');
      document.body.appendChild(linkContainer);

      /** Set inputs to some defaults. */
      const fooDestination = new print_preview.Destination(
          'FooPrinter', print_preview.DestinationType.LOCAL,
          print_preview.DestinationOrigin.LOCAL, 'Foo Printer',
          false /* isRecent */,
          print_preview.DestinationConnectionStatus.ONLINE);
      fooDestination.capabilities =
          print_preview_test_utils.getCddTemplate(fooDestination.id)
              .capabilities;
      linkContainer.destination = fooDestination;
      linkContainer.appKioskMode = false;
      linkContainer.disabled = false;
    });

    /** Tests that the system dialog link is hidden in App Kiosk mode. */
    test(assert(TestNames.HideInAppKioskMode), function() {
      const systemDialogLink = linkContainer.$.systemDialogLink;
      assertFalse(systemDialogLink.hidden);
      linkContainer.set('appKioskMode', true);
      assertTrue(systemDialogLink.hidden);
    });

    /**
     * Test that clicking the system dialog link click results in an event
     * firing, and the throbber appears on non-Windows.
     */
    test(assert(TestNames.SystemDialogLinkClick), function() {
      const promise =
          test_util.eventToPromise('print-with-system-dialog', linkContainer);
      const throbber = linkContainer.$.systemDialogThrobber;
      assertTrue(throbber.hidden);

      linkContainer.$.systemDialogLink.click();
      return promise.then(function() {
        assertEquals(cr.isWindows, throbber.hidden);
      });
    });

    /**
     * Test that if settings are invalid, the open in preview link is disabled
     * (if it exists), and that the system dialog link is disabled on Windows
     * and enabled on other platforms.
     */
    test(assert(TestNames.InvalidState), function() {
      const systemDialogLink = linkContainer.$.systemDialogLink;
      const openInPreviewLink =
          cr.isMac ? linkContainer.$.openPdfInPreviewLink : null;

      const validateLinkState = (link, disabled) => {
        assertFalse(link.hidden);
        assertEquals(!disabled, link.hasAttribute('actionable'));
        assertEquals(disabled, link.querySelector('button').disabled);
      };

      validateLinkState(systemDialogLink, false);
      if (cr.isMac)
        validateLinkState(openInPreviewLink, false);

      // Set disabled to true, indicating that there is a validation error or
      // printer error.
      linkContainer.disabled = true;
      validateLinkState(systemDialogLink, cr.isWindows);
      if (cr.isMac)
        validateLinkState(openInPreviewLink, true);
    });

    /**
     * Test that clicking the open in preview link correctly results in a
     * property change and that the throbber appears. Mac only.
     */
    test(assert(TestNames.OpenInPreviewLinkClick), function() {
      const throbber = linkContainer.$.openPdfInPreviewThrobber;
      assertTrue(throbber.hidden);
      const promise =
          test_util.eventToPromise('open-pdf-in-preview', linkContainer);

      linkContainer.$.openPdfInPreviewLink.click();
      return promise.then(function() {
        assertFalse(throbber.hidden);
      });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
