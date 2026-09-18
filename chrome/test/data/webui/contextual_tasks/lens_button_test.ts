// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/contextual_tasks_extension/lens_button_app.js';
import 'chrome://contextual-tasks/strings.m.js';

import {ExtensionBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {LensButtonAppElement} from 'chrome://contextual-tasks/contextual_tasks_extension/lens_button_app.js';
import {PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestExtensionBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

suite('LensButtonTest', () => {
  let app: LensButtonAppElement;
  let browserProxy: TestExtensionBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    browserProxy = new TestExtensionBrowserProxy();
    ExtensionBrowserProxyImpl.setInstance(browserProxy);

    loadTimeData.overrideValues({
      lensSearchButtonLabel: 'Search with Google Lens',
    });

    app = document.createElement('lens-button-app');
    document.body.appendChild(app);
    await microtasksFinished();
  });

  test('Button renders and is visible', () => {
    assertTrue(isVisible(app));
    const button = app.$.lensButton;
    assertTrue(isVisible(button));
    assertEquals('Search with Google Lens', button.getAttribute('aria-label'));
    assertEquals('Search with Google Lens', button.getAttribute('title'));
  });

  test('Supports active and disabled states', async () => {
    const button = app.$.lensButton;
    assertFalse(app.active);
    assertFalse(button.hasAttribute('active'));

    app.active = true;
    await microtasksFinished();
    assertTrue(app.hasAttribute('active'));
    assertTrue(button.hasAttribute('active'));

    assertFalse(app.disabled);
    assertFalse(button.disabled);

    app.disabled = true;
    await microtasksFinished();
    assertTrue(app.hasAttribute('disabled'));
    assertTrue(button.disabled);
  });

  test('Handles missing label gracefully', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      lensSearchButtonLabel: '',
    });

    const testApp = document.createElement('lens-button-app');
    document.body.appendChild(testApp);
    await microtasksFinished();

    const button = testApp.$.lensButton;
    assertEquals('', button.getAttribute('aria-label'));
    assertEquals('', button.getAttribute('title'));
  });

  test('Click triggers handleLensButtonClick', async () => {
    const mockHandler = TestMock.fromClass(ComposeboxPageHandlerRemote);
    app.setPageHandlerForTesting(mockHandler);

    app.$.lensButton.click();
    await mockHandler.whenCalled('handleLensButtonClick');
    assertEquals(1, mockHandler.getCallCount('handleLensButtonClick'));
  });

  test('Mousedown prevents default', () => {
    const event = new MouseEvent('mousedown', {cancelable: true});
    app.$.lensButton.dispatchEvent(event);
    assertTrue(event.defaultPrevented);
  });

  test(
      'Click on disabled button does not trigger handleLensButtonClick', () => {
        const mockHandler = TestMock.fromClass(ComposeboxPageHandlerRemote);
        app.setPageHandlerForTesting(mockHandler);

        app.disabled = true;
        app.$.lensButton.click();
        assertEquals(0, mockHandler.getCallCount('handleLensButtonClick'));
      });

  test(
      'Click triggers handleLensButtonClick without local toggle', async () => {
        const mockHandler = TestMock.fromClass(ComposeboxPageHandlerRemote);
        app.setPageHandlerForTesting(mockHandler);

        assertFalse(app.active);
        assertFalse(app.$.lensButton.hasAttribute('active'));
        assertEquals('false', app.$.lensButton.getAttribute('aria-pressed'));

        app.$.lensButton.click();
        await mockHandler.whenCalled('handleLensButtonClick');
        assertEquals(1, mockHandler.getCallCount('handleLensButtonClick'));

        // State is controlled by Mojo IPC from browser, not local click toggle.
        assertFalse(app.active);
        assertFalse(app.$.lensButton.hasAttribute('active'));
        assertEquals('false', app.$.lensButton.getAttribute('aria-pressed'));
      });

  test('Mojo onLensOverlayStateChanged synchronizes active state', async () => {
    assertFalse(app.active);
    assertFalse(app.$.lensButton.hasAttribute('active'));
    assertEquals('false', app.$.lensButton.getAttribute('aria-pressed'));

    browserProxy.callbackRouterRemote.onLensOverlayStateChanged(true);
    await microtasksFinished();
    assertTrue(app.active);
    assertTrue(app.$.lensButton.hasAttribute('active'));
    assertEquals('true', app.$.lensButton.getAttribute('aria-pressed'));

    browserProxy.callbackRouterRemote.onLensOverlayStateChanged(false);
    await microtasksFinished();
    assertFalse(app.active);
    assertFalse(app.$.lensButton.hasAttribute('active'));
    assertEquals('false', app.$.lensButton.getAttribute('aria-pressed'));
  });

  test('Disconnected element cleans up listeners', async () => {
    app.remove();
    await microtasksFinished();

    browserProxy.callbackRouterRemote.onLensOverlayStateChanged(true);
    await microtasksFinished();
    assertFalse(app.active);
  });
});
