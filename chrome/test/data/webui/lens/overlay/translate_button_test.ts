// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens-overlay/translate_button.js';

import {BrowserProxyImpl} from 'chrome-untrusted://lens-overlay/browser_proxy.js';
import {LanguageBrowserProxyImpl} from 'chrome-untrusted://lens-overlay/language_browser_proxy.js';
import {UserAction} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import {ShimmerControlRequester} from 'chrome-untrusted://lens-overlay/selection_utils.js';
import type {TranslateButtonElement} from 'chrome-untrusted://lens-overlay/translate_button.js';
import type {CrButtonElement} from 'chrome-untrusted://resources/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome-untrusted://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome-untrusted://webui-test/metrics_test_support.js';
import {flushTasks, waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome-untrusted://webui-test/test_util.js';

import {TestLanguageBrowserProxy} from './test_language_browser_proxy.js';
import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

suite('OverlayTranslateButton', function() {
  let overlayTranslateButtonElement: TranslateButtonElement;
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let testLanguageBrowserProxy: TestLanguageBrowserProxy;
  let metrics: MetricsTracker;

  // Remove CSS transitions to prevent race conditions due to an element not
  // being visible.
  function disableCssTransitions(element: TranslateButtonElement) {
    const sheet = new CSSStyleSheet();
    sheet.insertRule('* { transition: none !important; }');
    const shadow = element.shadowRoot!;
    shadow.adoptedStyleSheets = [sheet];
  }

  // Check if the element is rendered by checking if it is visible and its
  // opacity is not hiding it.
  function isRendered(el: HTMLElement) {
    return isVisible(el) && getComputedStyle(el).opacity !== '0';
  }

  setup(async () => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    // Set a test browser proxy so we can mock out the language setting calls.
    testLanguageBrowserProxy = new TestLanguageBrowserProxy();
    LanguageBrowserProxyImpl.setInstance(testLanguageBrowserProxy);

    overlayTranslateButtonElement = document.createElement('translate-button');
    document.body.appendChild(overlayTranslateButtonElement);
    disableCssTransitions(overlayTranslateButtonElement);
    metrics = fakeMetricsPrivate();
    await flushTasks();
  });

  test('TranslateButtonClick', async () => {
    assertFalse(isRendered(overlayTranslateButtonElement.$.languagePicker));

    const focusRegionEventPromise =
        eventToPromise('focus-region', document.body);
    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateEnableButton.click();
    // Clicking the translate button should focus the shimmer.
    const focusRegionEvent = await focusRegionEventPromise;
    assertEquals(
        focusRegionEvent.detail.requester, ShimmerControlRequester.TRANSLATE);


    // By default, we should send a translation request for source "auto" and
    // target language as defined by the proxy.
    const args = await testBrowserProxy.handler.whenCalled(
        'issueTranslateFullPageRequest');
    const sourceLanguage = args[0];
    const targetLanguage = args[1];
    const expectedTargetLanguage =
        await testLanguageBrowserProxy.getTranslateTargetLanguage();
    assertEquals(sourceLanguage, 'auto');
    assertEquals(targetLanguage, expectedTargetLanguage);

    // Language picker should now be visible.
    assertTrue(isRendered(overlayTranslateButtonElement.$.languagePicker));

    // Clicking again should toggle the language picker and send a end
    // translate mode request.
    const unfocusRegionEventPromise =
        eventToPromise('unfocus-region', document.body);
    overlayTranslateButtonElement.$.translateDisableButton.click();
    // Clicking the translate button again should unfocus the shimmer.
    await unfocusRegionEventPromise;
    const unfocusRegionEvent = await unfocusRegionEventPromise;
    assertEquals(
        unfocusRegionEvent.detail.requester, ShimmerControlRequester.TRANSLATE);
    assertEquals(
        1,
        testBrowserProxy.handler.getCallCount('issueTranslateFullPageRequest'));
    assertEquals(
        1,
        testBrowserProxy.handler.getCallCount('issueEndTranslateModeRequest'));
    assertEquals(
        1,
        metrics.count(
            'Lens.Overlay.Overlay.UserAction',
            UserAction.kTranslateButtonEnableAction));
    assertEquals(
        1,
        metrics.count(
            'Lens.Overlay.Overlay.UserAction',
            UserAction.kTranslateButtonDisableAction));
    assertEquals(
        1,
        metrics.count(
            'Lens.Overlay.Overlay.ByInvocationSource.AppMenu.UserAction',
            UserAction.kTranslateButtonEnableAction));
    assertEquals(
        1,
        metrics.count(
            'Lens.Overlay.Overlay.ByInvocationSource.AppMenu.UserAction',
            UserAction.kTranslateButtonDisableAction));

    // Language picker should be hidden again.
    assertFalse(isRendered(overlayTranslateButtonElement.$.languagePicker));
  });

  test('SourceLanguageButtonClick', () => {
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageButton));

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateEnableButton.click();

    // The source language button should be visible but the language picker menu
    // should not be visible.
    assertTrue(isVisible(overlayTranslateButtonElement.$.sourceLanguageButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));

    // Clicking the source language button should open the picker menu.
    overlayTranslateButtonElement.$.sourceLanguageButton.click();

    // The source language picker menu is visible.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));
  });

  test('TargetLanguageButtonClick', () => {
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguageButton));

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateEnableButton.click();

    // The target language button should be visible but the language picker menu
    // should not be visible.
    assertTrue(isVisible(overlayTranslateButtonElement.$.targetLanguageButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguagePickerMenu));

    // Clicking the target language button should open the picker menu.
    overlayTranslateButtonElement.$.targetLanguageButton.click();

    // The target language picker menu is visible.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.targetLanguagePickerMenu));
  });

  test('SourceLanguageMenuItemClick', async () => {
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageButton));

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateEnableButton.click();

    // The source language button should be visible but the language picker menu
    // should not be visible.
    assertTrue(isVisible(overlayTranslateButtonElement.$.sourceLanguageButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));

    // Clicking the source language button should open the picker menu.
    overlayTranslateButtonElement.$.sourceLanguageButton.click();

    // The source language picker menu is visible.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));

    // Get a menu item button from the source language picker menu to click.
    testBrowserProxy.handler.reset();
    const sourceLanguageMenuItem =
        overlayTranslateButtonElement.$.sourceLanguagePickerMenu
            .querySelector<CrButtonElement>(
                'cr-button:not(#sourceAutoDetectButton)');
    assertTrue(sourceLanguageMenuItem !== null);
    sourceLanguageMenuItem.click();

    // The source language button should be updated with the text of the new
    // source language.
    assertEquals(
        overlayTranslateButtonElement.$.sourceLanguageButton.innerText,
        sourceLanguageMenuItem.innerText.trim());

    // Verify a new translate full image request was sent.
    let args = await testBrowserProxy.handler.whenCalled(
        'issueTranslateFullPageRequest');
    let sourceLanguage = args[0];
    let targetLanguage = args[1];
    const expectedTargetLanguage =
        await testLanguageBrowserProxy.getTranslateTargetLanguage();
    assertEquals(sourceLanguage, 'en');
    assertEquals(targetLanguage, expectedTargetLanguage);

    // Both of the language picker menus should be hidden after this.
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguagePickerMenu));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));

    // Clicking the source language button should reopen the picker menu.
    overlayTranslateButtonElement.$.sourceLanguageButton.click();

    // The source language picker menu is visible.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));

    // Clicking the auto detect button should reset the source language button
    // text. Reset handler so we can recheck the request sent.
    testBrowserProxy.handler.reset();
    overlayTranslateButtonElement.$.sourceAutoDetectButton.click();
    assertEquals(
        overlayTranslateButtonElement.$.sourceLanguageButton.innerText,
        loadTimeData.getString('detectLanguage'));

    // Verify a new translate full image request was sent with auto detect.
    args = await testBrowserProxy.handler.whenCalled(
        'issueTranslateFullPageRequest');
    sourceLanguage = args[0];
    targetLanguage = args[1];
    assertEquals(sourceLanguage, 'auto');
    assertEquals(targetLanguage, expectedTargetLanguage);
    // Verify that two source languages changes were recorded in this test.
    assertEquals(
        2,
        metrics.count(
            'Lens.Overlay.Overlay.UserAction',
            UserAction.kTranslateSourceLanguageChanged));
    assertEquals(
        2,
        metrics.count(
            'Lens.Overlay.Overlay.ByInvocationSource.AppMenu.UserAction',
            UserAction.kTranslateSourceLanguageChanged));
  });

  test('TargetLanguageMenuItemClick', async () => {
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguageButton));

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateEnableButton.click();

    // The target language button should be visible but the language picker menu
    // should not be visible.
    assertTrue(isVisible(overlayTranslateButtonElement.$.targetLanguageButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguagePickerMenu));

    // Clicking the target language button should open the picker menu.
    overlayTranslateButtonElement.$.targetLanguageButton.click();

    // The target language picker menu is visible.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.targetLanguagePickerMenu));

    // Get a menu item button from the target language picker menu to click.
    // Make sure it's not the same menu item that is already selected.
    testBrowserProxy.handler.reset();
    const targetLanguageMenuItems =
        overlayTranslateButtonElement.$.targetLanguagePickerMenu
            .querySelectorAll<CrButtonElement>('cr-button');
    assertTrue(targetLanguageMenuItems !== null);
    const filteredMenuItems =
        Array.from(targetLanguageMenuItems).filter((item: CrButtonElement) => {
          return item.innerText !==
              overlayTranslateButtonElement.$.targetLanguageButton.innerText;
        });
    assertTrue(filteredMenuItems.length > 0);
    const targetLanguageMenuItem = filteredMenuItems[0] as CrButtonElement;
    targetLanguageMenuItem.click();

    // Verify the target language is updated in the new full image request.
    const args = await testBrowserProxy.handler.whenCalled(
        'issueTranslateFullPageRequest');
    const sourceLanguage = args[0];
    const targetLanguage = args[1];
    assertEquals(sourceLanguage, 'auto');
    assertEquals(targetLanguage, 'sw');

    // The target language button should be updated with the text of the new
    // target language.
    assertEquals(
        overlayTranslateButtonElement.$.targetLanguageButton.innerText,
        targetLanguageMenuItem.innerText.trim());

    // Both of the language picker menus should be hidden after this.
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguagePickerMenu));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));

    // Verify that target language change was recorded in this test.
    assertEquals(
        1,
        metrics.count(
            'Lens.Overlay.Overlay.UserAction',
            UserAction.kTranslateTargetLanguageChanged));
    assertEquals(
        1,
        metrics.count(
            'Lens.Overlay.Overlay.ByInvocationSource.AppMenu.UserAction',
            UserAction.kTranslateTargetLanguageChanged));
  });

  test('DetectedLanguageShown', async () => {
    // Receive content language from text layer.
    document.dispatchEvent(new CustomEvent(
        'received-content-language', {detail: {contentLanguage: 'sw'}}));
    await waitAfterNextRender(overlayTranslateButtonElement);

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateEnableButton.click();

    // Source language should show the detected language.
    assertEquals(
        overlayTranslateButtonElement.$.sourceLanguageButton.innerText,
        'Swahili');

    // Clicking the source language button should open the picker menu.
    overlayTranslateButtonElement.$.sourceLanguageButton.click();

    // Auto detect button should have the detected language.
    assertEquals(
        overlayTranslateButtonElement.$.menuDetectedLanguage.innerText,
        'Swahili');
  });
});
