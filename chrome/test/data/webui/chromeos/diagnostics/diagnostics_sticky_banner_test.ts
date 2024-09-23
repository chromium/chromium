// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://diagnostics/diagnostics_sticky_banner.js';

import {DiagnosticsStickyBannerElement} from 'chrome://diagnostics/diagnostics_sticky_banner.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('diagnosticsStickyBannerTestSuite', function() {
  let diagnosticsStickyBannerElement: DiagnosticsStickyBannerElement|null =
      null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    diagnosticsStickyBannerElement?.remove();
    diagnosticsStickyBannerElement = null;
  });

  function initializeDiagnosticsStickyBanner(): Promise<void> {
    // Add the sticky banner to the DOM.
    diagnosticsStickyBannerElement =
        document.createElement('diagnostics-sticky-banner');
    assert(diagnosticsStickyBannerElement);
    document.body.appendChild(diagnosticsStickyBannerElement);

    return flushTasks();
  }

  function getBanner(): Element {
    assert(diagnosticsStickyBannerElement);
    return strictQuery(
        '#banner', diagnosticsStickyBannerElement.shadowRoot, Element);
  }

  function getBannerMsg() {
    assert(diagnosticsStickyBannerElement);
    return strictQuery(
        '#bannerMsg', diagnosticsStickyBannerElement.shadowRoot, Element);
  }

  function getScrollClass(): string {
    assert(diagnosticsStickyBannerElement);
    return diagnosticsStickyBannerElement.getScrollingClassForTesting();
  }

  function getScrollTimerId(): number {
    assert(diagnosticsStickyBannerElement);
    return diagnosticsStickyBannerElement.getScrollTimerIdForTesting();
  }

  function setBannerMessage(message: string): Promise<void> {
    assert(diagnosticsStickyBannerElement);
    diagnosticsStickyBannerElement.bannerMessage = message;

    return flushTasks();
  }

  /**
   * Triggers 'dismiss-caution-banner' custom event.
   */
  function triggerDismissBannerEvent(): Promise<void> {
    window.dispatchEvent(new CustomEvent('dismiss-caution-banner', {
      bubbles: true,
      composed: true,
    }));

    return flushTasks();
  }

  /**
   * Triggers 'scroll' event.
   */
  function triggerScrollEvent(): Promise<void> {
    window.dispatchEvent(new CustomEvent('scroll', {
      bubbles: true,
      composed: true,
    }));

    return flushTasks();
  }

  /**
   * Triggers 'show-caution-banner' custom event with correctly configured event
   * detail object based on provided message.
   */
  function triggerShowBannerEvent(message: string): Promise<void> {
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
          assert(diagnosticsStickyBannerElement);
          assertEquals(
              bannerText1, diagnosticsStickyBannerElement.bannerMessage);
          assertTrue(isVisible(getBanner()));
          dx_utils.assertElementContainsText(getBannerMsg(), bannerText1);
        })
        .then(() => triggerShowBannerEvent(bannerText2))
        .then(() => {
          assert(diagnosticsStickyBannerElement);
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
          assert(diagnosticsStickyBannerElement);
          assertEquals('', diagnosticsStickyBannerElement.bannerMessage);
          assertFalse(isVisible(getBanner()));
          dx_utils.assertElementDoesNotContainText(getBannerMsg(), testMessage);
        });
  });

  test('BannerHandlesScrollEvent', () => {
    return initializeDiagnosticsStickyBanner()
        .then(() => {
          dx_utils.assertTextDoesNotContain(getScrollClass(), 'elevation-2');
          assertEquals(-1, getScrollTimerId());
        })
        .then(() => triggerScrollEvent())
        // Do not update if no banner message is set.
        .then(() => {
          dx_utils.assertTextDoesNotContain(getScrollClass(), 'elevation-2');
          assertEquals(-1, getScrollTimerId());
        })
        .then(() => setBannerMessage('Test Message'))
        .then(() => triggerScrollEvent())
        // First scroll initializes but does not update class.
        .then(() => {
          dx_utils.assertTextDoesNotContain(getScrollClass(), 'elevation-2');
          assertNotEquals(-1, getScrollTimerId());
        })
        // Subsequent scrolls ensure class name is set.
        .then(() => triggerScrollEvent())
        .then(() => {
          dx_utils.assertTextContains(getScrollClass(), 'elevation-2');
          assertNotEquals(-1, getScrollTimerId());
        });
  });
});
