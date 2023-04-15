// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addColorChangeListener, colorProviderChangeHandler, refreshColorCss, removeColorChangeListener} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('ColorChangeListenerTest', () => {
  setup(() => {
    document.body.innerHTML = getTrustedHTML`
      <link rel="stylesheet" href="chrome://theme/colors.css?sets=ui"/>`;
  });

  /**
   * Finds the most recently added link element in page whose href starts with
   * `matcher`, then returns the `param` search parameter on this elements href.
   */
  function getSearchParam(matcher: string, param: string) {
    const nodes =
        document.querySelectorAll<HTMLLinkElement>(`link[href*='${matcher}']`);
    // Since refreshColorCSS() won't remove the old link until the new link has
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

    // refreshColorCss() should append search params to the chrome://theme href.
    assertTrue(await refreshColorCss());

    let version = getSearchParam('chrome://theme/colors.css', 'version');
    assertNotEquals(version, null);
    const lastVersion = version;
    assertEquals(getSearchParam('chrome://theme/colors.css', 'sets'), 'ui');

    // Wait 1 millisecond before refresh. Otherwise the timestamp-based
    // version might not yet be updated.
    await new Promise(resolve => setTimeout(resolve, 1));
    assertTrue(await refreshColorCss());
    // refreshColorCss() should append search params to the colors CSS href.
    assertTrue(await refreshColorCss());

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

    assertFalse(await refreshColorCss());

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

    assertTrue(await refreshColorCss());

    assertTrue(!!getSearchParam('//theme/colors.css', 'version'));
  });

  test('HandlesCasesWhereColorsStylesheetIsNotSetCorrectly', async () => {
    // Handles the case where the link element exists but the attribute is
    // malformed.
    document.body.innerHTML =
        getTrustedHTML`<link rel="stylesheet" bad_href="chrome://theme/colors.css?sets=ui"/>`;
    assertFalse(await refreshColorCss());

    // Handles the case where the link element does not exist.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    assertFalse(await refreshColorCss());
  });

  test('RegistersColorChangeListener', async () => {
    let listenerCalledTimes = 0;
    addColorChangeListener(() => {
      listenerCalledTimes++;
    });

    // Emulate a color change event from the mojo pipe.
    await colorProviderChangeHandler();

    assertEquals(listenerCalledTimes, 1);
  });

  test('RemovesColorChangeListener', async () => {
    let listenerCalledTimes = 0;
    const listener = () => {
      listenerCalledTimes++;
    };
    addColorChangeListener(listener);

    // Emulate a color change event from the mojo pipe.
    await colorProviderChangeHandler();

    removeColorChangeListener(listener);

    // Emulate a color change event from the mojo pipe.
    await colorProviderChangeHandler();

    assertEquals(listenerCalledTimes, 1);
  });
});
