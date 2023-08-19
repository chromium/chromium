// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {COLORS_CSS_SELECTOR, ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
// <if expr="chromeos_ash">
import {COLOR_PROVIDER_CHANGED} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
// </if>

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
// clang-format on

suite('ColorChangeListenerTest', () => {
  let updater: ColorChangeUpdater;

  setup(() => {
    document.body.innerHTML = getTrustedHTML`
      <link rel="stylesheet" href="chrome://theme/colors.css?sets=ui"/>`;
    updater = ColorChangeUpdater.forDocument();
  });

  /**
   * Finds the most recently added link element in page whose href starts with
   * `matcher`, then returns the `param` search parameter on this elements href.
   */
  function getSearchParam(matcher: string, param: string) {
    const nodes =
        document.querySelectorAll<HTMLLinkElement>(`link[href*='${matcher}']`);
    // Since refreshColorsCss() won't remove the old link until the new link has
    // finished loading we may have multiple matches. Pick the last one to
    // ensure were getting the most recently added element.
    const node = nodes[nodes.length - 1];
    assertTrue(!!node);
    const href = node!.getAttribute('href');
    assertTrue(!!href);
    const params = new URLSearchParams(new URL(href!, location.href).search);
    return params.has(param) ? params.get(param) : null;
  }

  test('CorrectlyUpdatesColorsStylesheetURL', async () => {
    assertEquals(getSearchParam('chrome://theme/colors.css', 'version'), null);

    // refreshColorsCss() should append search params to the chrome://theme
    // href.
    assertTrue(await updater.refreshColorsCss());

    let version = getSearchParam('chrome://theme/colors.css', 'version');
    assertNotEquals(version, null);
    const lastVersion = version;
    assertEquals(getSearchParam('chrome://theme/colors.css', 'sets'), 'ui');

    // Wait 1 millisecond before refresh. Otherwise the timestamp-based
    // version might not yet be updated.
    await new Promise(resolve => setTimeout(resolve, 1));
    assertTrue(await updater.refreshColorsCss());
    // refreshColorsCss() should append search params to the colors CSS href.
    assertTrue(await updater.refreshColorsCss());

    version = getSearchParam('chrome://theme/colors.css', 'version');
    assertTrue(!!version);
    assertNotEquals(version, lastVersion);
    assertEquals(getSearchParam('chrome://theme/colors.css', 'sets'), 'ui');
  });

  test('IgnoresNonTargetStylesheetURLs', async () => {
    document.body.innerHTML = getTrustedHTML`
      <link rel="stylesheet" href="chrome://resources/colors.css"/>`;
    assertEquals(
        getSearchParam('chrome://resources/colors.css', 'version'), null);

    assertFalse(await updater.refreshColorsCss());

    assertEquals(
        getSearchParam('chrome://resources/colors.css', 'version'), null);
  });

  test('HandlesRelativeURLs', async () => {
    // Handles the case where the link element exists but the attribute is
    // malformed.
    document.body.innerHTML = getTrustedHTML`
      <link rel="stylesheet" href="//theme/colors.css?sets=ui"/>
    `;
    assertEquals(getSearchParam('//theme/colors.css', 'version'), null);

    assertTrue(await updater.refreshColorsCss());

    assertTrue(!!getSearchParam('//theme/colors.css', 'version'));
  });

  test('HandlesCasesWhereColorsStylesheetIsNotSetCorrectly', async () => {
    // Handles the case where the link element exists but the attribute is
    // malformed.
    document.body.innerHTML =
        getTrustedHTML`<link rel="stylesheet" bad_href="chrome://theme/colors.css?sets=ui"/>`;
    assertFalse(await updater.refreshColorsCss());

    // Handles the case where the link element does not exist.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    assertFalse(await updater.refreshColorsCss());
  });

  test('HandlesCasesWhereColorCssIsRefreshedMultipleTimes', async () => {
    // Emulate multiple color change events from the mojo pipe. Do not await
    // the first call so that multiple events are in flight at the same time.
    await Promise.all(
        [updater.onColorProviderChanged(), updater.onColorProviderChanged()]);

    // Verify only one colors.css exists.
    assertEquals(1, document.querySelectorAll(COLORS_CSS_SELECTOR).length);
  });

  // <if expr="chromeos_ash">
  test('AddAndRemoveColorProviderChangedListener', async () => {
    let listenerCalledTimes = 0;
    const listener = () => listenerCalledTimes++;
    updater.eventTarget.addEventListener(COLOR_PROVIDER_CHANGED, listener);

    // Emulate a color change event from the mojo pipe.
    await updater.onColorProviderChanged();
    assertEquals(listenerCalledTimes, 1);

    updater.eventTarget.removeEventListener(COLOR_PROVIDER_CHANGED, listener);

    // Emulate a color change event from the mojo pipe.
    await updater.onColorProviderChanged();
    assertEquals(listenerCalledTimes, 1);
  });
  // </if>
});
