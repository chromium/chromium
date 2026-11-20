// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import type {LhsBrandmarkElement} from 'chrome://new-tab-page/new_tab_page.js';
import {ThreadsRailAction, ThreadsRailExpandedAction, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

const AIM_THREADS_GOOGLE_SEARCH_LABEL = 'Google Search';
const GOOGLE_BASE_URL = 'https://www.google.com/';
const DEFAULT_DARK_WORDMARK_FILL = 'rgb(255, 255, 255)';
const THEMED_DARK_WORDMARK_FILL = 'rgb(230, 232, 240)';
const EXPECTED_LIGHT_LETTER_FILLS = [
  'rgb(49, 134, 255)',
  'rgb(252, 65, 61)',
  'rgb(255, 190, 0)',
  'rgb(49, 134, 255)',
  'rgb(0, 175, 87)',
  'rgb(252, 65, 61)',
];

suite('NewTabPageLhsBrandmarkTest', () => {
  let brandmark: LhsBrandmarkElement;
  let metrics: MetricsTracker;
  let windowProxy: TestMock<WindowProxy>;

  async function createBrandmark(): Promise<LhsBrandmarkElement> {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    brandmark = document.createElement('ntp-lhs-brandmark');
    document.body.appendChild(brandmark);
    await microtasksFinished();
    return brandmark;
  }

  setup(async () => {
    document.documentElement.style.setProperty('--cr-animations-disabled', '1');
    loadTimeData.overrideValues({
      aimThreadsGoogleSearchLabel: AIM_THREADS_GOOGLE_SEARCH_LABEL,
      googleBaseUrl: GOOGLE_BASE_URL,
    });
    metrics = fakeMetricsPrivate();
    windowProxy = installMock(WindowProxy);
    await createBrandmark();
  });

  teardown(() => {
    document.documentElement.style.removeProperty('--cr-animations-disabled');
    document.documentElement.style.removeProperty(
        '--color-new-tab-page-threads-rail-brand-primary-font');
    document.documentElement.style.removeProperty(
        '--lhs-folded-pointer-events');
  });

  test('renders Super G button and 6-path Google wordmark SVG', () => {
    const logoButton = brandmark.$.logoButton;
    assertEquals(
        AIM_THREADS_GOOGLE_SEARCH_LABEL, logoButton.getAttribute('title'));
    assertFalse(logoButton.hasAttribute('aria-label'));
    assertTrue(logoButton.hasAttribute('suppress-rtl-flip'));

    const wordmark = brandmark.$.wordmark;
    assertEquals(GOOGLE_BASE_URL, wordmark.getAttribute('href'));
    assertEquals(
        AIM_THREADS_GOOGLE_SEARCH_LABEL, wordmark.getAttribute('aria-label'));

    const paths = wordmark.querySelectorAll('svg path');
    assertEquals(EXPECTED_LIGHT_LETTER_FILLS.length, paths.length);
    for (const path of paths) {
      assertTrue(path instanceof SVGPathElement);
      assertEquals('http://www.w3.org/2000/svg', path.namespaceURI);
      const bbox = path.getBBox();
      assertTrue(bbox.width > 0 && bbox.height > 0);
    }
  });

  test(
      'transitions between Super G and Google wordmark in light and dark mode',
      async () => {
        const logoButton = brandmark.$.logoButton;
        const wordmark = brandmark.$.wordmark;
        const letters =
            Array.from(wordmark.querySelectorAll<SVGPathElement>('svg path'));
        assertEquals(EXPECTED_LIGHT_LETTER_FILLS.length, letters.length);

        // Collapsed resting state: Super G visible, wordmark hidden and tucked.
        assertFalse(brandmark.expanded);
        assertFalse(logoButton.inert);
        assertEquals('visible', getComputedStyle(logoButton).visibility);
        assertEquals('1', getComputedStyle(logoButton).opacity);
        assertTrue(wordmark.inert);
        assertEquals('hidden', getComputedStyle(wordmark).visibility);
        assertEquals('0', getComputedStyle(letters[0]!).opacity);
        assertEquals(
            EXPECTED_LIGHT_LETTER_FILLS[0], getComputedStyle(letters[0]!).fill);

        // Overriding --cr-animations-disabled to 0 enables collapse transitions
        // and reverse stagger delays (30ms -> 0ms).
        document.documentElement.style.setProperty(
            '--cr-animations-disabled', '0');
        assertEquals(
            '0.065s, 0.26s, 0s',
            getComputedStyle(logoButton).transitionDuration);
        assertEquals(
            '0.08s, 0.055s, 0.08s',
            getComputedStyle(letters[0]!).transitionDuration);
        assertEquals('0.03s', getComputedStyle(letters[0]!).transitionDelay);
        assertEquals('0s', getComputedStyle(letters[5]!).transitionDelay);
        document.documentElement.style.setProperty(
            '--cr-animations-disabled', '1');

        // Expand in Light Mode: Super G hides and 6 letters unfurl in brand
        // colors.
        brandmark.expanded = true;
        await microtasksFinished();
        assertTrue(logoButton.inert);
        assertEquals('hidden', getComputedStyle(logoButton).visibility);
        assertEquals('0', getComputedStyle(logoButton).opacity);
        assertFalse(wordmark.inert);
        assertEquals('visible', getComputedStyle(wordmark).visibility);
        letters.forEach((letter, index) => {
          assertEquals('1', getComputedStyle(letter).opacity);
          assertEquals('none', getComputedStyle(letter).transform);
          assertEquals(
              EXPECTED_LIGHT_LETTER_FILLS[index],
              getComputedStyle(letter).fill);
        });

        // Dark Mode expanded state falls back to #fff when the theme token is
        // unset, and uses --color-new-tab-page-threads-rail-brand-primary-font
        // when provided by ColorProvider.
        brandmark.isDark = true;
        await microtasksFinished();
        for (const letter of letters) {
          assertEquals(
              DEFAULT_DARK_WORDMARK_FILL, getComputedStyle(letter).fill);
        }

        document.documentElement.style.setProperty(
            '--color-new-tab-page-threads-rail-brand-primary-font',
            THEMED_DARK_WORDMARK_FILL);
        for (const letter of letters) {
          assertEquals(
              THEMED_DARK_WORDMARK_FILL, getComputedStyle(letter).fill);
        }

        // Overriding --cr-animations-disabled to 0 enables expand transitions
        // and forward stagger delays (40..150ms, fill 120..270ms).
        document.documentElement.style.setProperty(
            '--cr-animations-disabled', '0');
        assertEquals(
            '0.05s, 0.14s, 0s',
            getComputedStyle(logoButton).transitionDuration);
        assertEquals(
            '0.42s, 0.095s, 0.28s',
            getComputedStyle(letters[0]!).transitionDuration);
        assertEquals(
            '0.04s, 0.04s, 0.12s',
            getComputedStyle(letters[0]!).transitionDelay);
        assertEquals(
            '0.15s, 0.15s, 0.27s',
            getComputedStyle(letters[5]!).transitionDelay);
        document.documentElement.style.setProperty(
            '--cr-animations-disabled', '1');

        // Collapse resets letters back to brand fills and restores Super G.
        brandmark.expanded = false;
        await microtasksFinished();
        assertFalse(logoButton.inert);
        assertEquals('visible', getComputedStyle(logoButton).visibility);
        assertEquals('1', getComputedStyle(logoButton).opacity);
        assertTrue(wordmark.inert);
        assertEquals('hidden', getComputedStyle(wordmark).visibility);
        assertEquals(
            EXPECTED_LIGHT_LETTER_FILLS[0], getComputedStyle(letters[0]!).fill);
      });

  test('honors --lhs-folded-pointer-events on #logoButton', () => {
    const logoButton = brandmark.$.logoButton;
    assertEquals('auto', getComputedStyle(logoButton).pointerEvents);

    document.documentElement.style.setProperty(
        '--lhs-folded-pointer-events', 'none');
    assertEquals('none', getComputedStyle(logoButton).pointerEvents);

    document.documentElement.style.removeProperty(
        '--lhs-folded-pointer-events');
    assertEquals('auto', getComputedStyle(logoButton).pointerEvents);
  });

  test(
      'navigates to googleBaseUrl on primary clicks without bubbling',
      async () => {
        let parentClicked = false;
        brandmark.addEventListener('click', () => {
          parentClicked = true;
        });

        const logoButton = brandmark.$.logoButton;
        logoButton.click();
        assertEquals(1, windowProxy.getCallCount('navigate'));
        assertEquals(GOOGLE_BASE_URL, windowProxy.getArgs('navigate')[0]);
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.ThreadsRail.Action',
                ThreadsRailAction.LOGO_CLICKED));
        assertEquals(0, metrics.count('NewTabPage.ThreadsRail.ExpandedAction'));
        assertFalse(parentClicked);

        brandmark.expanded = true;
        await microtasksFinished();

        const wordmark = brandmark.$.wordmark;
        // Verify onLogoClick_ leaves default <a> new-tab navigation intact on
        // Ctrl+Click, then cancel at the element boundary so browser_tests
        // does not open an external tab.
        let sawUncancelledModifiedClick = false;
        wordmark.addEventListener('click', e => {
          sawUncancelledModifiedClick = !e.defaultPrevented;
          e.preventDefault();
        }, {once: true});
        const modifiedClick = new MouseEvent('click', {
          bubbles: true,
          cancelable: true,
          composed: true,
          ctrlKey: true,
        });
        wordmark.dispatchEvent(modifiedClick);
        assertTrue(sawUncancelledModifiedClick);
        assertEquals(1, windowProxy.getCallCount('navigate'));
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.ThreadsRail.ExpandedAction',
                ThreadsRailExpandedAction.LOGO_CLICKED));
        assertFalse(parentClicked);

        parentClicked = false;
        wordmark.click();
        assertEquals(2, windowProxy.getCallCount('navigate'));
        assertEquals(GOOGLE_BASE_URL, windowProxy.getArgs('navigate')[1]);
        assertEquals(1, metrics.count('NewTabPage.ThreadsRail.Action'));
        assertEquals(
            2,
            metrics.count(
                'NewTabPage.ThreadsRail.ExpandedAction',
                ThreadsRailExpandedAction.LOGO_CLICKED));
        assertFalse(parentClicked);
      });

  test('ignores invalid googleBaseUrl', async () => {
    loadTimeData.overrideValues({
      googleBaseUrl: 'javascript:alert(1)',
    });
    await createBrandmark();

    assertFalse(brandmark.$.wordmark.hasAttribute('href'));
    brandmark.$.logoButton.click();
    brandmark.expanded = true;
    await microtasksFinished();
    brandmark.$.wordmark.click();

    assertEquals(0, windowProxy.getCallCount('navigate'));
    assertEquals(0, metrics.count('NewTabPage.ThreadsRail.Action'));
    assertEquals(0, metrics.count('NewTabPage.ThreadsRail.ExpandedAction'));
  });
});
