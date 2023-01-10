// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {DiagnosticsStickyBannerElement} from 'chrome://diagnostics/diagnostics_sticky_banner.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isVisible} from '../test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('diagnosticsStickyBannerTestSuite', function() {
  /** @type {?DiagnosticsStickyBannerElement} */
  let diagnosticsStickyBannerElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    diagnosticsStickyBannerElement.remove();
    diagnosticsStickyBannerElement = null;
  });

  /** @return {!Promise} */
  function initializeDiagnosticsStickyBanner() {
    assertFalse(!!diagnosticsStickyBannerElement);

    // Add the sticky banner to the DOM.
    diagnosticsStickyBannerElement =
        /** @type {!DiagnosticsStickyBannerElement} */ (
            document.createElement('diagnostics-sticky-banner'));
    assertTrue(!!diagnosticsStickyBannerElement);
    document.body.appendChild(diagnosticsStickyBannerElement);

    return flushTasks();
  }

  /** @return {!Element} */
  function getBanner() {
    assertTrue(!!diagnosticsStickyBannerElement);

    return /** @type {!Element} */ (
        diagnosticsStickyBannerElement.shadowRoot.querySelector('#banner'));
  }

  /** @return {!Element} */
  function getBannerMsg() {
    assertTrue(!!diagnosticsStickyBannerElement);

    return /** @type {!Element} */ (
        diagnosticsStickyBannerElement.shadowRoot.querySelector('#bannerMsg'));
  }

  /**
   * @suppress {visibility}
   * @return {string}
   */
  function getScrollClass_() {
    return diagnosticsStickyBannerElement.scrollingClass;
  }

  /**
   * @suppress {visibility}
   * @return {number}
   */
  function getScrollTimerId_() {
    return diagnosticsStickyBannerElement.scrollTimerId;
  }

  /**
   * @param {string} message
   * @return {!Promise}
   */
  function setBannerMessage(message) {
    assertTrue(!!diagnosticsStickyBannerElement);
    diagnosticsStickyBannerElement.bannerMessage = message;

    return flushTasks();
  }

  /**
   * Triggers 'dismiss-caution-banner' custom event.
   * @return {!Promise}
   */
  function triggerDismissBannerEvent() {
    window.dispatchEvent(new CustomEvent('dismiss-caution-banner', {
      bubbles: true,
      composed: true,
    }));

    return flushTasks();
  }

  /**
   * Triggers 'scroll' event.
   * @return {!Promise}
   */
  function triggerScrollEvent() {
    window.dispatchEvent(new CustomEvent('scroll', {
      bubbles: true,
      composed: true,
    }));

    return flushTasks();
  }

  /**
   * Triggers 'show-caution-banner' custom event with correctly configured event
   * detail object based on provided message.
   * @param {string} message
   * @return {!Promise}
   */
  function triggerShowBannerEvent(message) {
    window.dispatchEvent(new CustomEvent('show-caution-banner', {
      bubbles: true,
      composed: true,
      detail: {message},
    }));

    return flushTasks();
  }

  test('BannerInitializedCorrectly', () => {
    return initializeDiagnosticsStickyBanner().then(() => {
      assertFalse(isVisible(getBanner()));
    });
  });

  test('BannerShowsWhenMessageSetToNonEmptyString', () => {
    const testMessage = 'Infomational banner';
    return initializeDiagnosticsStickyBanner()
        .then(() => setBannerMessage(testMessage))
        .then(() => {
          assertTrue(isVisible(getBanner()));
          dx_utils.assertElementContainsText(getBannerMsg(), testMessage);
        })
        .then(() => setBannerMessage(''))
        .then(() => {
          assertFalse(isVisible(getBanner()));
          dx_utils.assertElementDoesNotContainText(getBannerMsg(), testMessage);
        });
  });

  test('BannerHandlesShowCautionBannerEvent', () => {
    const bannerText1 = 'Infomational banner 1';
    const bannerText2 = 'Infomational banner 2';
    return initializeDiagnosticsStickyBanner()
        .then(() => triggerShowBannerEvent(bannerText1))
        .then(() => {
          assertEquals(
              bannerText1, diagnosticsStickyBannerElement.bannerMessage);
          assertTrue(isVisible(getBanner()));
          dx_utils.assertElementContainsText(getBannerMsg(), bannerText1);
        })
        .then(() => triggerShowBannerEvent(bannerText2))
        .then(() => {
          assertEquals(
              bannerText2, diagnosticsStickyBannerElement.bannerMessage);
          assertTrue(isVisible(getBanner()));
          dx_utils.assertElementContainsText(getBannerMsg(), bannerText2);
        });
  });

  test('BannerHandlesDismissCautionBannerEvent', () => {
    const testMessage = 'Infomational banner';
    return initializeDiagnosticsStickyBanner()
        .then(() => triggerShowBannerEvent(testMessage))
        .then(() => {
          dx_utils.assertElementContainsText(getBannerMsg(), testMessage);
        })
        .then(() => triggerDismissBannerEvent())
        .then(() => {
          assertEquals('', diagnosticsStickyBannerElement.bannerMessage);
          assertFalse(isVisible(getBanner()));
          dx_utils.assertElementDoesNotContainText(getBannerMsg(), testMessage);
        });
  });

  test('BannerHandlesScrollEvent', () => {
    return initializeDiagnosticsStickyBanner()
        .then(() => {
          dx_utils.assertTextDoesNotContain(getScrollClass_(), 'elevation-2');
          assertEquals(-1, getScrollTimerId_());
        })
        .then(() => triggerScrollEvent())
        // Do not update if no banner message is set.
        .then(() => {
          dx_utils.assertTextDoesNotContain(getScrollClass_(), 'elevation-2');
          assertEquals(-1, getScrollTimerId_());
        })
        .then(() => setBannerMessage('Test Message'))
        .then(() => triggerScrollEvent())
        // First scroll initializes but does not update class.
        .then(() => {
          dx_utils.assertTextDoesNotContain(getScrollClass_(), 'elevation-2');
          assertNotEquals(-1, getScrollTimerId_());
        })
        // Subsequent scrolls ensure class name is set.
        .then(() => triggerScrollEvent())
        .then(() => {
          dx_utils.assertTextContains(getScrollClass_(), 'elevation-2');
          assertNotEquals(-1, getScrollTimerId_());
        });
  });
});
