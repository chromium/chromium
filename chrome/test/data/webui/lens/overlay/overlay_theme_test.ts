// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens-overlay/lens_overlay_app.js';

import {BrowserProxyImpl} from 'chrome-untrusted://lens-overlay/browser_proxy.js';
import type {LensOverlayAppElement} from 'chrome-untrusted://lens-overlay/lens_overlay_app.js';
import {assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {hasStyle} from 'chrome-untrusted://webui-test/test_util.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

suite('OverlayTheme', () => {
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let lensOverlayElement: LensOverlayAppElement;

  setup(() => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    lensOverlayElement = document.createElement('lens-overlay-app');
    document.body.appendChild(lensOverlayElement);
    return waitAfterNextRender(lensOverlayElement);
  });

  // Verify theme CSS color vars are set to the Fallback colors until theme
  // data is received.
  test('ThemeCSSColorVars', async () => {
    // Verify Fallback color vars.
    const appContainerBefore =
        lensOverlayElement.shadowRoot!.querySelector('.app-container');
    assertTrue(!!appContainerBefore);
    assertTrue(hasStyle(appContainerBefore, '--color-primary', '#181c22'));
    assertTrue(
        hasStyle(appContainerBefore, '--color-shader-layer-1', '#5b5e66'));
    assertTrue(
        hasStyle(appContainerBefore, '--color-shader-layer-2', '#8e9199'));
    assertTrue(
        hasStyle(appContainerBefore, '--color-shader-layer-3', '#a6c8ff'));
    assertTrue(
        hasStyle(appContainerBefore, '--color-shader-layer-4', '#eef0f9'));
    assertTrue(
        hasStyle(appContainerBefore, '--color-shader-layer-5', '#a8abb3'));
    assertTrue(hasStyle(appContainerBefore, '--color-scrim', '#181c22'));
    assertTrue(hasStyle(appContainerBefore, '--color-scrim-rgb', '24, 28, 34'));
    assertTrue(hasStyle(
        appContainerBefore, '--color-surface-container-highest-light',
        '#e0e2eb'));
    assertTrue(hasStyle(
        appContainerBefore, '--color-surface-container-highest-dark',
        '#43474e'));
    assertTrue(
        hasStyle(appContainerBefore, '--color-selection-element', '#eef0f9'));

    // Send Grape colors.
    testBrowserProxy.page.themeReceived({
      primary: {value: 0xff6018d6},
      shaderLayer1: {value: 0xff8169bb},
      shaderLayer2: {value: 0xff9b83d7},
      shaderLayer3: {value: 0xffc9fdd5},
      shaderLayer4: {value: 0xffe9ddff},
      shaderLayer5: {value: 0xffd0bcff},
      scrim: {value: 0xff1e1928},
      surfaceContainerHighestLight: {value: 0xffe9ddff},
      surfaceContainerHighestDark: {value: 0xff4c3f69},
      selectionElement: {value: 0xfff3ebfa},
    });
    await waitAfterNextRender(lensOverlayElement);

    // Verify Grape color vars.
    const appContainerAfter =
        lensOverlayElement.shadowRoot!.querySelector('.app-container');
    assertTrue(!!appContainerAfter);
    assertTrue(hasStyle(appContainerAfter, '--color-primary', '#6018d6'));
    assertTrue(
        hasStyle(appContainerAfter, '--color-shader-layer-1', '#8169bb'));
    assertTrue(
        hasStyle(appContainerAfter, '--color-shader-layer-2', '#9b83d7'));
    assertTrue(
        hasStyle(appContainerAfter, '--color-shader-layer-3', '#c9fdd5'));
    assertTrue(
        hasStyle(appContainerAfter, '--color-shader-layer-4', '#e9ddff'));
    assertTrue(
        hasStyle(appContainerAfter, '--color-shader-layer-5', '#d0bcff'));
    assertTrue(hasStyle(appContainerAfter, '--color-scrim', '#1e1928'));
    assertTrue(hasStyle(appContainerAfter, '--color-scrim-rgb', '30, 25, 40'));
    assertTrue(hasStyle(
        appContainerAfter, '--color-surface-container-highest-light',
        '#e9ddff'));
    assertTrue(hasStyle(
        appContainerAfter, '--color-surface-container-highest-dark',
        '#4c3f69'));
    assertTrue(
        hasStyle(appContainerAfter, '--color-selection-element', '#f3ebfa'));
  });
});
