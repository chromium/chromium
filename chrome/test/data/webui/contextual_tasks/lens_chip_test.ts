// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/contextual_tasks_extension/lens_chip_app.js';
import 'chrome://contextual-tasks/strings.m.js';

import {ExtensionBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {LensChipAppElement} from 'chrome://contextual-tasks/contextual_tasks_extension/lens_chip_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestExtensionBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

suite('LensChipTest', () => {
  let app: LensChipAppElement;
  let browserProxy: TestExtensionBrowserProxy;

  const MOCK_DATA_URI =
      'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAA' +
      'ADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    browserProxy = new TestExtensionBrowserProxy();
    ExtensionBrowserProxyImpl.setInstance(browserProxy);

    loadTimeData.overrideValues({
      lensRegionChipLabel: 'Selected image region',
      lensRegionChipDismissA11yLabel: 'Remove selected region',
    });
  });

  test('Chip renders preview when preview fetch succeeds', async () => {
    browserProxy.handler.setLensCropPreviewResult(MOCK_DATA_URI);

    app = document.createElement('lens-chip-app');
    document.body.appendChild(app);
    await microtasksFinished();

    await browserProxy.handler.whenCalled('getLensCropPreview');
    assertEquals(1, browserProxy.handler.getCallCount('getLensCropPreview'));

    assertEquals(MOCK_DATA_URI, app.getDataUriForTesting());
    const chipRoot = app.shadowRoot?.querySelector('#chipRoot');
    assertTrue(!!chipRoot);
    assertTrue(isVisible(chipRoot));

    const cropImage =
        app.shadowRoot?.querySelector<HTMLImageElement>('#cropImage');
    assertTrue(!!cropImage);
    assertTrue(isVisible(cropImage));
    assertEquals(MOCK_DATA_URI, cropImage.src);
    assertEquals('Selected image region', cropImage.alt);

    const closeButton =
        app.shadowRoot?.querySelector<HTMLElement>('#closeButton');
    assertTrue(!!closeButton);
    assertTrue(isVisible(closeButton));
    assertEquals('cr:close', closeButton.getAttribute('iron-icon'));
    assertEquals(
        'Remove selected region', closeButton.getAttribute('aria-label'));
    assertEquals('Remove selected region', closeButton.getAttribute('title'));
    assertEquals('8px', getComputedStyle(app).borderRadius);
    assertEquals('0', getComputedStyle(closeButton).opacity);
    assertEquals('none', getComputedStyle(closeButton).pointerEvents);
  });

  test(
      'Chip does not render preview when preview fetch returns null',
      async () => {
        browserProxy.handler.setLensCropPreviewResult(null);

        app = document.createElement('lens-chip-app');
        document.body.appendChild(app);
        await microtasksFinished();

        await browserProxy.handler.whenCalled('getLensCropPreview');
        const cropImage = app.shadowRoot?.querySelector('#cropImage');
        assertEquals(null, cropImage);
        const chipRoot = app.shadowRoot?.querySelector('#chipRoot');
        assertEquals(null, chipRoot);
      });

  test(
      'Clicking close button fires close-chip event and calls removeLensCrop',
      async () => {
        browserProxy.handler.setLensCropPreviewResult(MOCK_DATA_URI);

        app = document.createElement('lens-chip-app');
        document.body.appendChild(app);
        await microtasksFinished();

        await browserProxy.handler.whenCalled('getLensCropPreview');
        assertEquals(MOCK_DATA_URI, app.getDataUriForTesting());

        const closeButton =
            app.shadowRoot?.querySelector<HTMLElement>('#closeButton');
        assertTrue(!!closeButton);

        const closeEventPromise = eventToPromise('close-chip', app);
        closeButton.click();
        await closeEventPromise;

        await browserProxy.handler.whenCalled('removeLensCrop');
        assertEquals(1, browserProxy.handler.getCallCount('removeLensCrop'));
        assertEquals('', app.getDataUriForTesting());
      });

  test(
      'Chip updates preview when onLensCropUpdated is dispatched', async () => {
        browserProxy.handler.setLensCropPreviewResult(null);

        app = document.createElement('lens-chip-app');
        document.body.appendChild(app);
        await microtasksFinished();

        const UPDATED_DATA_URI =
            'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg' +
            '0kAAAAFElEQVR42mNk+M/wHwMDAwMDkAAAAP//DwYFBQAAAABJRU5ErkJggg==';

        browserProxy.callbackRouterRemote.onLensCropUpdated(UPDATED_DATA_URI);
        await microtasksFinished();

        assertEquals(UPDATED_DATA_URI, app.getDataUriForTesting());
        const cropImage =
            app.shadowRoot?.querySelector<HTMLImageElement>('#cropImage');
        assertTrue(!!cropImage);
        assertTrue(isVisible(cropImage));
        assertEquals(UPDATED_DATA_URI, cropImage.src);
      });

  test('Dark mode reflects attribute correctly', async () => {
    app = document.createElement('lens-chip-app');
    document.body.appendChild(app);
    await microtasksFinished();

    assertFalse(app.getDarkModeForTesting());
    assertFalse(app.hasAttribute('dark-mode'));

    app.setDarkModeForTesting(true);
    await microtasksFinished();
    assertTrue(app.hasAttribute('dark-mode'));

    app.setDarkModeForTesting(false);
    await microtasksFinished();
    assertFalse(app.hasAttribute('dark-mode'));
  });
});
