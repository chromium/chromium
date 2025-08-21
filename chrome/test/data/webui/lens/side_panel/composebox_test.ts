// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/side_panel/side_panel_app.js';

import type {LensSidePanelAppElement} from 'chrome-untrusted://lens/side_panel/side_panel_app.js';
import {SidePanelBrowserProxyImpl} from 'chrome-untrusted://lens/side_panel/side_panel_browser_proxy.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';

import {TestLensSidePanelBrowserProxy} from './test_side_panel_browser_proxy.js';

// This is a slow function, but is guaranteed to return false if the element is
// not actually visible. isVisible() is fast, but may return true even if the
// element is has opacity: 0 or is hidden because its parents opacity is 0.
function isTrulyVisible(element: HTMLElement): boolean {
  let el: HTMLElement|null = element;
  while (el) {
    const style = window.getComputedStyle(el);
    if (style.display === 'none' || style.visibility === 'hidden' ||
        style.opacity === '0') {
      return false;
    }
    el = el.parentElement;
  }
  return true;
}

// Returns a promise that resolves when the element has finished any
// transition. If a property is provided, only resolves when that property has
// finished transitioning.
function getTransitionEndPromise(
    element: HTMLElement, property?: string): Promise<void> {
  return new Promise<void>(
      resolve =>
          element.addEventListener('transitionend', (e: TransitionEvent) => {
            if (!property || e.propertyName === property) {
              resolve();
            }
          }));
}
suite('Composebox', () => {
  let testBrowserProxy: TestLensSidePanelBrowserProxy;
  let lensSidePanelElement: LensSidePanelAppElement;

  // Returns the composebox element.
  async function setupTest(): Promise<HTMLElement> {
    testBrowserProxy = new TestLensSidePanelBrowserProxy();
    SidePanelBrowserProxyImpl.setInstance(testBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    lensSidePanelElement = document.createElement('lens-side-panel-app');
    document.body.appendChild(lensSidePanelElement);

    await waitAfterNextRender(lensSidePanelElement);
    const composebox =
        lensSidePanelElement.shadowRoot!.querySelector('ntp-composebox');
    assertTrue(!!composebox);
    return composebox;
  }

  test('HidesComposeboxWhenDisabled', async () => {
    loadTimeData.overrideValues({enableAimSearchbox: false});
    const composebox = await setupTest();

    assertFalse(isVisible(composebox));
    assertTrue(isVisible(lensSidePanelElement.$.searchboxContainer));
  });

  test('ShowsComposeboxWhenEnabled', async () => {
    loadTimeData.overrideValues({enableAimSearchbox: true});
    const composebox = await setupTest();

    assertTrue(isVisible(composebox));
    assertFalse(isVisible(lensSidePanelElement.$.searchboxContainer));
  });

  test('HidesFileInputs', async () => {
    loadTimeData.overrideValues({enableAimSearchbox: true});
    const composebox = await setupTest();

    const imageUploadButton =
        composebox.shadowRoot!.querySelector('#imageUploadButton');
    const fileUploadButton =
        composebox.shadowRoot!.querySelector('#fileUploadButton');

    // The hide-file-inputs_ attribute is added in side_panel_app.html, so
    // the file input buttons should not be visible.
    assertFalse(isVisible(imageUploadButton));
    assertFalse(isVisible(fileUploadButton));
  });

  test('IsCollapsible', async () => {
    loadTimeData.overrideValues({enableAimSearchbox: true});
    const composebox = await setupTest();

    const animatedElement =
        composebox.shadowRoot!.querySelector<HTMLElement>('#composebox');
    assertTrue(!!animatedElement);

    const initialHeight = composebox.offsetHeight;
    assertTrue(initialHeight > 0);

    const input = composebox.shadowRoot!.querySelector<HTMLTextAreaElement>(
        'textarea#input');
    assertTrue(!!input);

    // Focusing the input should expand the composebox.
    const expandPromise =
        getTransitionEndPromise(animatedElement, 'max-height');
    input.focus();
    await expandPromise;
    const expandedHeight = composebox.offsetHeight;
    assertTrue(expandedHeight > initialHeight);

    // Blurring the input should collapse the composebox.
    const collapsePromise =
        getTransitionEndPromise(animatedElement, 'max-height');
    input.blur();
    await collapsePromise;
    assertEquals(initialHeight, composebox.offsetHeight);
  });

  test('ButtonsHideAndShow', async () => {
    loadTimeData.overrideValues({enableAimSearchbox: true});
    const composebox = await setupTest();

    // Grab the buttons to do visibility checks.
    const submitButton =
        composebox.shadowRoot!.querySelector<HTMLElement>('#submitIcon');
    const cancelButton =
        composebox.shadowRoot!.querySelector<HTMLElement>('#cancelIcon');
    assertTrue(!!submitButton);
    assertTrue(!!cancelButton);

    // The buttons should not be visible initially while the composebox is
    // collapsed.
    assertFalse(isTrulyVisible(submitButton));
    assertFalse(isTrulyVisible(cancelButton));

    // Grab the input to focus it.
    const input = composebox.shadowRoot!.querySelector<HTMLTextAreaElement>(
        'textarea#input');
    assertTrue(!!input);

    // Focusing the input should expand the composebox.
    const submitShowPromise =
        getTransitionEndPromise(submitButton.parentElement!);
    const cancelShowPromise = getTransitionEndPromise(cancelButton);
    input.focus();
    await Promise.all([submitShowPromise, cancelShowPromise]);

    // The buttons should be visible now that the composebox is expanded.
    assertTrue(isTrulyVisible(submitButton));
    assertTrue(isTrulyVisible(cancelButton));

    // Blur the input to collapse the composebox.
    const submitHidePromise =
        getTransitionEndPromise(submitButton.parentElement!);
    const cancelHidePromise = getTransitionEndPromise(cancelButton);
    input.blur();
    await Promise.all([submitHidePromise, cancelHidePromise]);

    // The buttons should not be visible again.
    assertFalse(isTrulyVisible(submitButton));
    assertFalse(isTrulyVisible(cancelButton));
  });
});
