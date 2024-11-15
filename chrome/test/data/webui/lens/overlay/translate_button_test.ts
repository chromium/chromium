// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens-overlay/translate_button.js';

import {BrowserProxyImpl} from 'chrome-untrusted://lens-overlay/browser_proxy.js';
import {LanguageBrowserProxyImpl} from 'chrome-untrusted://lens-overlay/language_browser_proxy.js';
import {UserAction} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import type {LensPageRemote} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import {ShimmerControlRequester} from 'chrome-untrusted://lens-overlay/selection_utils.js';
import type {Language} from 'chrome-untrusted://lens-overlay/translate.mojom-webui.js';
import type {TranslateButtonElement} from 'chrome-untrusted://lens-overlay/translate_button.js';
import type {CrButtonElement} from 'chrome-untrusted://resources/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome-untrusted://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome-untrusted://webui-test/metrics_test_support.js';
import {flushTasks, waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome-untrusted://webui-test/test_util.js';

import {TestLanguageBrowserProxy} from './test_language_browser_proxy.js';
import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

const TEST_FETCH_LANGUAGES = [
  {
    languageCode: 'es',
    name: 'Spanish',
  },
  {
    languageCode: 'fr',
    name: 'French',
  },
];
const TEST_FETCH_LANGUAGES_OTHER = [
  {
    languageCode: 'ar',
    name: 'Arabic',
  },
  {
    languageCode: 'pt',
    name: 'Portuguese',
  },
];

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

suite('OverlayTranslateButton', function() {
  let callbackRouterRemote: LensPageRemote;
  let overlayTranslateButtonElement: TranslateButtonElement;
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let testLanguageBrowserProxy: TestLanguageBrowserProxy;
  let metrics: MetricsTracker;

  setup(async () => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    callbackRouterRemote =
        testBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    // Set a test browser proxy so we can mock out the language setting calls.
    testLanguageBrowserProxy = new TestLanguageBrowserProxy();
    LanguageBrowserProxyImpl.setInstance(testLanguageBrowserProxy);

    overlayTranslateButtonElement = document.createElement('translate-button');
    document.body.appendChild(overlayTranslateButtonElement);
    disableCssTransitions(overlayTranslateButtonElement);
    metrics = fakeMetricsPrivate();
    await flushTasks();
    await waitAfterNextRender(overlayTranslateButtonElement);
  });

  test('TranslateButtonClick', async () => {
    assertFalse(isRendered(overlayTranslateButtonElement.$.languagePicker));

    const expectedTargetLanguage =
        await testLanguageBrowserProxy.getTranslateTargetLanguage();
    const focusRegionEventPromise =
        eventToPromise('focus-region', document.body);
    let translateModeStateChangePromise =
        eventToPromise('translate-mode-state-changed', document.body);
    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateEnableButton.click();

    // Clicking the translate button should focus the shimmer.
    const focusRegionEvent = await focusRegionEventPromise;
    assertEquals(
        focusRegionEvent.detail.requester, ShimmerControlRequester.TRANSLATE);

    // Translate mode state change event should have been fired.
    let translateModeStateChangeEvent = await translateModeStateChangePromise;
    assertTrue(translateModeStateChangeEvent.detail.shouldUnselectWords);
    assertTrue(translateModeStateChangeEvent.detail.translateModeEnabled);
    assertEquals(
        translateModeStateChangeEvent.detail.targetLanguage,
        expectedTargetLanguage);

    // By default, we should send a translation request for source "auto" and
    // target language as defined by the proxy.
    const args = await testBrowserProxy.handler.whenCalled(
        'issueTranslateFullPageRequest');
    const sourceLanguage = args[0];
    const targetLanguage = args[1];
    assertEquals(sourceLanguage, 'auto');
    assertEquals(targetLanguage, expectedTargetLanguage);

    // Language picker should now be visible.
    assertTrue(isRendered(overlayTranslateButtonElement.$.languagePicker));

    // Clicking again should toggle the language picker and send a end
    // translate mode request.
    const unfocusRegionEventPromise =
        eventToPromise('unfocus-region', document.body);
    translateModeStateChangePromise =
        eventToPromise('translate-mode-state-changed', document.body);
    overlayTranslateButtonElement.$.translateDisableButton.click();
    // Clicking the translate button again should unfocus the shimmer.
    await unfocusRegionEventPromise;
    const unfocusRegionEvent = await unfocusRegionEventPromise;
    assertEquals(
        unfocusRegionEvent.detail.requester, ShimmerControlRequester.TRANSLATE);

    // Translate mode state change event should have been fired.
    translateModeStateChangeEvent = await translateModeStateChangePromise;
    assertTrue(translateModeStateChangeEvent.detail.shouldUnselectWords);
    assertFalse(translateModeStateChangeEvent.detail.translateModeEnabled);
    assertEquals(
        translateModeStateChangeEvent.detail.targetLanguage,
        expectedTargetLanguage);

    assertEquals(
        1,
        testBrowserProxy.handler.getCallCount(
            'maybeCloseTranslateFeaturePromo'));
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

  test('SourceLanguageButtonClick', async () => {
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageButton));

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateEnableButton.click();

    // The source language button should be visible but the language picker menu
    // should not be visible.
    assertTrue(isVisible(overlayTranslateButtonElement.$.sourceLanguageButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));

    const languagePickerOpenEventPromise =
        eventToPromise('language-picker-opened', document);
    // Clicking the source language button should open the picker menu.
    overlayTranslateButtonElement.$.sourceLanguageButton.click();
    await languagePickerOpenEventPromise;

    // The source language picker menu is visible.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));
  });

  test('TargetLanguageButtonClick', async () => {
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguageButton));

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateEnableButton.click();

    // The target language button should be visible but the language picker menu
    // should not be visible.
    assertTrue(isVisible(overlayTranslateButtonElement.$.targetLanguageButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguagePickerMenu));

    const languagePickerOpenEventPromise =
        eventToPromise('language-picker-opened', document);
    // Clicking the target language button should open the picker menu.
    overlayTranslateButtonElement.$.targetLanguageButton.click();
    await languagePickerOpenEventPromise;

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
        overlayTranslateButtonElement.$.allSourceLanguagesMenu
            .querySelector<CrButtonElement>(
                'cr-button:not(#sourceAutoDetectButton)');
    assertTrue(sourceLanguageMenuItem !== null);
    let languagePickerCloseEventPromise =
        eventToPromise('language-picker-closed', document);
    sourceLanguageMenuItem.click();
    await languagePickerCloseEventPromise;

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
    languagePickerCloseEventPromise =
        eventToPromise('language-picker-closed', document);
    overlayTranslateButtonElement.$.sourceAutoDetectButton.click();
    assertEquals(
        overlayTranslateButtonElement.$.sourceLanguageButton.innerText,
        loadTimeData.getString('detectLanguage'));
    await languagePickerCloseEventPromise;

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
        overlayTranslateButtonElement.$.allTargetLanguagesMenu
            .querySelectorAll<CrButtonElement>('cr-button');
    assertTrue(targetLanguageMenuItems !== null);
    const filteredMenuItems =
        Array.from(targetLanguageMenuItems).filter((item: CrButtonElement) => {
          return item.innerText !==
              overlayTranslateButtonElement.$.targetLanguageButton.innerText;
        });
    assertTrue(filteredMenuItems.length > 0);
    const targetLanguageMenuItem = filteredMenuItems[0] as CrButtonElement;
    const languagePickerCloseEventPromise =
        eventToPromise('language-picker-closed', document);
    targetLanguageMenuItem.click();
    await languagePickerCloseEventPromise;

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

  test('SetTranslateMode', async () => {
    // Verify translate mode is disabled.
    assertFalse(isRendered(overlayTranslateButtonElement.$.languagePicker));
    assertFalse(
        isRendered(overlayTranslateButtonElement.$.sourceLanguageButton));
    assertFalse(
        isRendered(overlayTranslateButtonElement.$.targetLanguageButton));

    const firstSourceLanguage = 'auto';
    const firstTargetLanguage = 'sw';
    let translateModeStateChangePromise =
        eventToPromise('translate-mode-state-changed', document.body);
    let focusRegionEventPromise = eventToPromise('focus-region', document.body);
    callbackRouterRemote.setTranslateMode(
        firstSourceLanguage, firstTargetLanguage);
    await waitAfterNextRender(overlayTranslateButtonElement);

    // Language picker should now be visible.
    assertTrue(isRendered(overlayTranslateButtonElement.$.languagePicker));
    assertTrue(
        isRendered(overlayTranslateButtonElement.$.sourceLanguageButton));
    assertTrue(
        isRendered(overlayTranslateButtonElement.$.targetLanguageButton));

    // Language buttons should have languages set according to setTranslateMode.
    assertEquals(
        overlayTranslateButtonElement.$.sourceLanguageButton.innerText,
        loadTimeData.getString('detectLanguage'));
    assertEquals(
        overlayTranslateButtonElement.$.targetLanguageButton.innerText,
        'Swahili');

    // The shimmer should be focused in translate.
    let focusRegionEvent = await focusRegionEventPromise;
    assertEquals(
        focusRegionEvent.detail.requester, ShimmerControlRequester.TRANSLATE);

    // Translate mode state change event should have been fired.
    let translateModeStateChangeEvent = await translateModeStateChangePromise;
    assertTrue(translateModeStateChangeEvent.detail.shouldUnselectWords);
    assertTrue(translateModeStateChangeEvent.detail.translateModeEnabled);
    assertEquals(
        translateModeStateChangeEvent.detail.targetLanguage,
        firstTargetLanguage);

    const secondSourceLanguage = 'sw';
    const secondTargetLanguage = 'en';
    translateModeStateChangePromise =
        eventToPromise('translate-mode-state-changed', document.body);
    callbackRouterRemote.setTranslateMode(
        secondSourceLanguage, secondTargetLanguage);
    await waitAfterNextRender(overlayTranslateButtonElement);

    // Language buttons should have languages set according to setTranslateMode.
    assertEquals(
        overlayTranslateButtonElement.$.sourceLanguageButton.innerText,
        'Swahili');
    assertEquals(
        overlayTranslateButtonElement.$.targetLanguageButton.innerText,
        'English');

    // Translate mode state change event should have been fired.
    translateModeStateChangeEvent = await translateModeStateChangePromise;
    assertTrue(translateModeStateChangeEvent.detail.shouldUnselectWords);
    assertTrue(translateModeStateChangeEvent.detail.translateModeEnabled);
    assertEquals(
        translateModeStateChangeEvent.detail.targetLanguage,
        secondTargetLanguage);

    let unfocusRegionEventPromise =
        eventToPromise('unfocus-region', document.body);
    translateModeStateChangePromise =
        eventToPromise('translate-mode-state-changed', document.body);
    // `setTranslateMode` is called with empty languages when there is a
    // non-translate selection to go back to via back button. This means the
    // translate mode should be disabled.
    callbackRouterRemote.setTranslateMode('', '');
    await waitAfterNextRender(overlayTranslateButtonElement);
    // Verify translate mode is disabled.
    assertFalse(isRendered(overlayTranslateButtonElement.$.languagePicker));
    assertFalse(
        isRendered(overlayTranslateButtonElement.$.sourceLanguageButton));
    assertFalse(
        isRendered(overlayTranslateButtonElement.$.targetLanguageButton));

    // The shimmer should be unfocus translate.
    let unfocusRegionEvent = await unfocusRegionEventPromise;
    assertEquals(
        unfocusRegionEvent.detail.requester, ShimmerControlRequester.TRANSLATE);

    // Translate mode state change event should have been fired.
    translateModeStateChangeEvent = await translateModeStateChangePromise;
    assertFalse(translateModeStateChangeEvent.detail.shouldUnselectWords);
    assertFalse(translateModeStateChangeEvent.detail.translateModeEnabled);
    assertEquals(
        translateModeStateChangeEvent.detail.targetLanguage,
        secondTargetLanguage);

    const thirdSourceLanguage = 'en';
    const thirdTargetLanguage = 'sw';
    focusRegionEventPromise = eventToPromise('focus-region', document.body);
    translateModeStateChangePromise =
        eventToPromise('translate-mode-state-changed', document.body);
    callbackRouterRemote.setTranslateMode(
        thirdSourceLanguage, thirdTargetLanguage);
    await waitAfterNextRender(overlayTranslateButtonElement);

    // Language picker should now be visible.
    assertTrue(isRendered(overlayTranslateButtonElement.$.languagePicker));
    assertTrue(
        isRendered(overlayTranslateButtonElement.$.sourceLanguageButton));
    assertTrue(
        isRendered(overlayTranslateButtonElement.$.targetLanguageButton));

    // Language buttons should have languages set according to setTranslateMode.
    assertEquals(
        overlayTranslateButtonElement.$.sourceLanguageButton.innerText,
        'English');
    assertEquals(
        overlayTranslateButtonElement.$.targetLanguageButton.innerText,
        'Swahili');

    // The shimmer should be focused in translate.
    focusRegionEvent = await focusRegionEventPromise;
    assertEquals(
        focusRegionEvent.detail.requester, ShimmerControlRequester.TRANSLATE);

    // Translate mode state change event should have been fired.
    translateModeStateChangeEvent = await translateModeStateChangePromise;
    assertTrue(translateModeStateChangeEvent.detail.shouldUnselectWords);
    assertTrue(translateModeStateChangeEvent.detail.translateModeEnabled);
    assertEquals(
        translateModeStateChangeEvent.detail.targetLanguage,
        thirdTargetLanguage);

    unfocusRegionEventPromise = eventToPromise('unfocus-region', document.body);
    translateModeStateChangePromise =
        eventToPromise('translate-mode-state-changed', document.body);
    callbackRouterRemote.setTranslateMode('', '');
    await waitAfterNextRender(overlayTranslateButtonElement);
    // Verify translate mode is disabled.
    assertFalse(isRendered(overlayTranslateButtonElement.$.languagePicker));
    assertFalse(
        isRendered(overlayTranslateButtonElement.$.sourceLanguageButton));
    assertFalse(
        isRendered(overlayTranslateButtonElement.$.targetLanguageButton));

    // The shimmer should be unfocus translate.
    unfocusRegionEvent = await unfocusRegionEventPromise;
    assertEquals(
        unfocusRegionEvent.detail.requester, ShimmerControlRequester.TRANSLATE);

    // Translate mode state change event should have been fired.
    translateModeStateChangeEvent = await translateModeStateChangePromise;
    assertFalse(translateModeStateChangeEvent.detail.shouldUnselectWords);
    assertFalse(translateModeStateChangeEvent.detail.translateModeEnabled);
    assertEquals(
        translateModeStateChangeEvent.detail.targetLanguage,
        thirdTargetLanguage);

    // Verify the call count of starting translate requests.
    assertEquals(
        3,
        testBrowserProxy.handler.getCallCount('issueTranslateFullPageRequest'));
  });

  test('SetTranslateModeToSameLanguages', async () => {
    // Verify translate mode is disabled.
    assertFalse(isRendered(overlayTranslateButtonElement.$.languagePicker));
    assertFalse(
        isRendered(overlayTranslateButtonElement.$.sourceLanguageButton));
    assertFalse(
        isRendered(overlayTranslateButtonElement.$.targetLanguageButton));

    const sourceLanguage = 'auto';
    const targetLanguage = 'sw';
    const focusRegionEventPromise =
        eventToPromise('focus-region', document.body);
    callbackRouterRemote.setTranslateMode(sourceLanguage, targetLanguage);
    await waitAfterNextRender(overlayTranslateButtonElement);

    // Language picker should now be visible.
    assertTrue(isRendered(overlayTranslateButtonElement.$.languagePicker));
    assertTrue(
        isRendered(overlayTranslateButtonElement.$.sourceLanguageButton));
    assertTrue(
        isRendered(overlayTranslateButtonElement.$.targetLanguageButton));

    // Language buttons should have languages set according to setTranslateMode.
    assertEquals(
        overlayTranslateButtonElement.$.sourceLanguageButton.innerText,
        loadTimeData.getString('detectLanguage'));
    assertEquals(
        overlayTranslateButtonElement.$.targetLanguageButton.innerText,
        'Swahili');

    // The shimmer should be focused in translate.
    const focusRegionEvent = await focusRegionEventPromise;
    assertEquals(
        focusRegionEvent.detail.requester, ShimmerControlRequester.TRANSLATE);

    callbackRouterRemote.setTranslateMode(sourceLanguage, targetLanguage);
    await waitAfterNextRender(overlayTranslateButtonElement);

    // Language buttons should have languages set according to setTranslateMode.
    assertEquals(
        overlayTranslateButtonElement.$.sourceLanguageButton.innerText,
        loadTimeData.getString('detectLanguage'));
    assertEquals(
        overlayTranslateButtonElement.$.targetLanguageButton.innerText,
        'Swahili');

    // Verify the call count of starting translate requests.
    assertEquals(
        1,
        testBrowserProxy.handler.getCallCount('issueTranslateFullPageRequest'));
  });

  test('TranslateTabAndFocusOrder', async () => {
    // Verify translate mode is disabled.
    assertFalse(isRendered(overlayTranslateButtonElement.$.languagePicker));
    assertFalse(
        isRendered(overlayTranslateButtonElement.$.sourceLanguageButton));
    assertFalse(
        isRendered(overlayTranslateButtonElement.$.targetLanguageButton));

    // Only the enable button should be tabbable.
    assertEquals(
        0, overlayTranslateButtonElement.$.translateEnableButton.tabIndex);
    assertEquals(
        -1, overlayTranslateButtonElement.$.translateDisableButton.tabIndex);
    assertEquals(
        -1, overlayTranslateButtonElement.$.sourceLanguageButton.tabIndex);
    assertEquals(
        -1, overlayTranslateButtonElement.$.targetLanguageButton.tabIndex);

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateEnableButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // After clicking the translate button the source language button should
    // have focus and the enable button should not be tabbable.
    assertEquals(
        -1, overlayTranslateButtonElement.$.translateEnableButton.tabIndex);
    assertEquals(
        0, overlayTranslateButtonElement.$.translateDisableButton.tabIndex);
    assertEquals(
        0, overlayTranslateButtonElement.$.sourceLanguageButton.tabIndex);
    assertEquals(
        0, overlayTranslateButtonElement.$.targetLanguageButton.tabIndex);
    assertEquals(
        overlayTranslateButtonElement.shadowRoot!.activeElement,
        overlayTranslateButtonElement.$.sourceLanguageButton);

    // Clicking the source language button should open the picker menu.
    overlayTranslateButtonElement.$.sourceLanguageButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);
    // The source language picker menu is visible.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));

    // None of the language picker buttons should be tabbable.
    assertEquals(
        -1, overlayTranslateButtonElement.$.translateEnableButton.tabIndex);
    assertEquals(
        -1, overlayTranslateButtonElement.$.translateDisableButton.tabIndex);
    assertEquals(
        -1, overlayTranslateButtonElement.$.sourceLanguageButton.tabIndex);
    assertEquals(
        -1, overlayTranslateButtonElement.$.targetLanguageButton.tabIndex);

    // The back button in the source language picker menu should have focus.
    assertEquals(
        overlayTranslateButtonElement.shadowRoot!.activeElement,
        overlayTranslateButtonElement.$.sourceLanguagePickerBackButton);

    // Clicking the back button should close the picker menu and return focus to
    // the source language button.
    overlayTranslateButtonElement.$.sourceLanguagePickerBackButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);
    assertEquals(
        -1, overlayTranslateButtonElement.$.translateEnableButton.tabIndex);
    assertEquals(
        0, overlayTranslateButtonElement.$.translateDisableButton.tabIndex);
    assertEquals(
        0, overlayTranslateButtonElement.$.sourceLanguageButton.tabIndex);
    assertEquals(
        0, overlayTranslateButtonElement.$.targetLanguageButton.tabIndex);
    assertEquals(
        overlayTranslateButtonElement.shadowRoot!.activeElement,
        overlayTranslateButtonElement.$.sourceLanguageButton);

    // Clicking the target language button should open the picker menu.
    overlayTranslateButtonElement.$.targetLanguageButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);
    // The target language picker menu is visible.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.targetLanguagePickerMenu));

    // None of the language picker buttons should be tabbable.
    assertEquals(
        -1, overlayTranslateButtonElement.$.translateEnableButton.tabIndex);
    assertEquals(
        -1, overlayTranslateButtonElement.$.translateDisableButton.tabIndex);
    assertEquals(
        -1, overlayTranslateButtonElement.$.sourceLanguageButton.tabIndex);
    assertEquals(
        -1, overlayTranslateButtonElement.$.targetLanguageButton.tabIndex);
    // The back button in the target language picker menu should have focus.
    assertEquals(
        overlayTranslateButtonElement.shadowRoot!.activeElement,
        overlayTranslateButtonElement.$.targetLanguagePickerBackButton);

    // Clicking the back button should close the picker menu and return focus to
    // the target language button.
    overlayTranslateButtonElement.$.targetLanguagePickerBackButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);
    assertEquals(
        -1, overlayTranslateButtonElement.$.translateEnableButton.tabIndex);
    assertEquals(
        0, overlayTranslateButtonElement.$.translateDisableButton.tabIndex);
    assertEquals(
        0, overlayTranslateButtonElement.$.sourceLanguageButton.tabIndex);
    assertEquals(
        0, overlayTranslateButtonElement.$.targetLanguageButton.tabIndex);
    assertEquals(
        overlayTranslateButtonElement.shadowRoot!.activeElement,
        overlayTranslateButtonElement.$.targetLanguageButton);

    // Clicking the translate disable button should close translate mode and
    // return focus to the translate enable button.
    overlayTranslateButtonElement.$.translateDisableButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // Verify translate mode is disabled.
    assertFalse(isRendered(overlayTranslateButtonElement.$.languagePicker));
    assertFalse(
        isRendered(overlayTranslateButtonElement.$.sourceLanguageButton));
    assertFalse(
        isRendered(overlayTranslateButtonElement.$.targetLanguageButton));

    // Only the enable button should be tabbable again.
    assertEquals(
        0, overlayTranslateButtonElement.$.translateEnableButton.tabIndex);
    assertEquals(
        -1, overlayTranslateButtonElement.$.translateDisableButton.tabIndex);
    assertEquals(
        -1, overlayTranslateButtonElement.$.sourceLanguageButton.tabIndex);
    assertEquals(
        -1, overlayTranslateButtonElement.$.targetLanguageButton.tabIndex);

    // Verify focus on the translate enable button.
    assertEquals(
        overlayTranslateButtonElement.shadowRoot!.activeElement,
        overlayTranslateButtonElement.$.translateEnableButton);
  });
});

suite('OverlayTranslateButtonLanguages', function() {
  let overlayTranslateButtonElement: TranslateButtonElement;
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let testLanguageBrowserProxy: TestLanguageBrowserProxy;
  let metrics: MetricsTracker;

  // Helper function for adding the translate button element. Needed so we can
  // modify the browser proxy response per unit test since the languages are
  // fetched whenever this element is ready / rendered.
  async function addTranslateButtonElement() {
    overlayTranslateButtonElement = document.createElement('translate-button');
    document.body.appendChild(overlayTranslateButtonElement);
    disableCssTransitions(overlayTranslateButtonElement);
    await flushTasks();
    await waitAfterNextRender(overlayTranslateButtonElement);
  }

  // Change the input element's value and dispatch an input changed event soon
  // after.
  function dispatchInputEvent(inputElement: HTMLInputElement, query: string) {
    inputElement.value = query;
    inputElement.dispatchEvent(new InputEvent('input'));
  }

  setup(async () => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      'shouldFetchSupportedLanguages': true,
      'translateSourceLanguages': 'ar,en,es,fr,pt,sw',
      'translateTargetLanguages':
          '',  // source languages are added to target languages.
      'languagesCacheTimeout': 604800000,  // a week in milliseconds
    });

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    // Set a test browser proxy so we can mock out the language setting calls.
    testLanguageBrowserProxy = new TestLanguageBrowserProxy();
    LanguageBrowserProxyImpl.setInstance(testLanguageBrowserProxy);

    metrics = fakeMetricsPrivate();

    // Clear window localStorage.
    window.localStorage.clear();
    await flushTasks();
  });

  test('UseServerLanguageListOnSuccess', async () => {
    testBrowserProxy.handler.setLanguagesToFetchForTesting(
        'en-US', TEST_FETCH_LANGUAGES, TEST_FETCH_LANGUAGES);
    await addTranslateButtonElement();

    assertEquals(testLanguageBrowserProxy.getStoredLocale(), 'en-US');
    assertDeepEquals(
        testLanguageBrowserProxy.getStoredSourceLanguages(),
        TEST_FETCH_LANGUAGES);
    assertDeepEquals(
        testLanguageBrowserProxy.getStoredTargetLanguages(),
        TEST_FETCH_LANGUAGES);

    const clientLanguageNames =
        (await testLanguageBrowserProxy.getClientLanguageList())
            .map((lang: Language) => lang.name);
    const serverLanguageNames =
        TEST_FETCH_LANGUAGES.map((lang: Language) => lang.name);

    // Check the language picker list as it should be using the server language
    // list.
    const sourceLanguageMenuItems =
        Array.from(overlayTranslateButtonElement.$.allSourceLanguagesMenu
                       .querySelectorAll<CrButtonElement>(
                           'cr-button:not(#sourceAutoDetectButton)'));
    assertTrue(sourceLanguageMenuItems !== null);
    assertTrue(sourceLanguageMenuItems.length > 0);
    sourceLanguageMenuItems.map((button: CrButtonElement) => {
      assertTrue(serverLanguageNames.includes(button.innerText.trim()));
      assertFalse(clientLanguageNames.includes(button.innerText.trim()));
    });

    const targetLanguageMenuItems =
        Array.from(overlayTranslateButtonElement.$.allTargetLanguagesMenu
                       .querySelectorAll<CrButtonElement>('cr-button'));
    assertTrue(targetLanguageMenuItems !== null);
    assertTrue(targetLanguageMenuItems.length > 0);
    targetLanguageMenuItems.map((button: CrButtonElement) => {
      assertTrue(serverLanguageNames.includes(button.innerText.trim()));
      assertFalse(clientLanguageNames.includes(button.innerText.trim()));
    });

    assertEquals(
        1, testBrowserProxy.handler.getCallCount('fetchSupportedLanguages'));
  });

  test('TestServerLanguageFilters', async () => {
    loadTimeData.overrideValues(
        {'translateSourceLanguages': 'es', 'translateTargetLanguages': 'fr'});
    testBrowserProxy.handler.setLanguagesToFetchForTesting(
        'en-US', TEST_FETCH_LANGUAGES, TEST_FETCH_LANGUAGES);
    await addTranslateButtonElement();

    assertEquals(testLanguageBrowserProxy.getStoredLocale(), 'en-US');
    assertDeepEquals(
        testLanguageBrowserProxy.getStoredSourceLanguages(),
        TEST_FETCH_LANGUAGES);
    assertDeepEquals(
        testLanguageBrowserProxy.getStoredTargetLanguages(),
        TEST_FETCH_LANGUAGES);

    const serverLanguageNames =
        TEST_FETCH_LANGUAGES.map((lang: Language) => lang.name);

    // Check the language picker list as it should be using the server language
    // list. Make sure it only uses the correct filtered languages.
    const sourceLanguageMenuItems =
        Array.from(overlayTranslateButtonElement.$.allSourceLanguagesMenu
                       .querySelectorAll<CrButtonElement>(
                           'cr-button:not(#sourceAutoDetectButton)'));
    assertTrue(sourceLanguageMenuItems !== null);
    assertEquals(sourceLanguageMenuItems.length, 1);
    sourceLanguageMenuItems.map((button: CrButtonElement) => {
      assertEquals(button.innerText.trim(), 'Spanish');
    });

    const targetLanguageMenuItems =
        Array.from(overlayTranslateButtonElement.$.allTargetLanguagesMenu
                       .querySelectorAll<CrButtonElement>('cr-button'));
    assertTrue(targetLanguageMenuItems !== null);
    assertEquals(targetLanguageMenuItems.length, 2);
    targetLanguageMenuItems.map((button: CrButtonElement) => {
      assertTrue(serverLanguageNames.includes(button.innerText.trim()));
    });

    assertEquals(
        1, testBrowserProxy.handler.getCallCount('fetchSupportedLanguages'));
  });

  test('TestClientLanguageFilters', async () => {
    loadTimeData.overrideValues(
        {'translateSourceLanguages': 'sw', 'translateTargetLanguages': 'en'});
    testBrowserProxy.handler.setLanguagesToFetchForTesting('en-US', [], []);
    await addTranslateButtonElement();

    // If the fetch fails, it will return empty language lists. In this case,
    // the localStorageProxy will not have stored anything.
    assertEquals(testLanguageBrowserProxy.getStoredLocale(), '');
    assertEquals(testLanguageBrowserProxy.getStoredSourceLanguages().length, 0);
    assertEquals(testLanguageBrowserProxy.getStoredTargetLanguages().length, 0);

    const clientLanguageNames =
        (await testLanguageBrowserProxy.getClientLanguageList())
            .map((lang: Language) => lang.name);

    // Check the language picker list as it should be using the client language
    // list since a server language list could not be fetched. Make sure it only
    // uses the correct filtered languages.
    const sourceLanguageMenuItems =
        Array.from(overlayTranslateButtonElement.$.allSourceLanguagesMenu
                       .querySelectorAll<CrButtonElement>(
                           'cr-button:not(#sourceAutoDetectButton)'));
    assertTrue(sourceLanguageMenuItems !== null);
    assertEquals(sourceLanguageMenuItems.length, 1);
    sourceLanguageMenuItems.map((button: CrButtonElement) => {
      assertEquals(button.innerText.trim(), 'Swahili');
    });

    const targetLanguageMenuItems =
        Array.from(overlayTranslateButtonElement.$.allTargetLanguagesMenu
                       .querySelectorAll<CrButtonElement>('cr-button'));
    assertTrue(targetLanguageMenuItems !== null);
    assertEquals(targetLanguageMenuItems.length, 2);
    targetLanguageMenuItems.map((button: CrButtonElement) => {
      assertTrue(clientLanguageNames.includes(button.innerText.trim()));
    });

    assertEquals(
        1, testBrowserProxy.handler.getCallCount('fetchSupportedLanguages'));
  });

  test('UseClientLanguageListIfFetchFails', async () => {
    testBrowserProxy.handler.setLanguagesToFetchForTesting('en-US', [], []);
    await addTranslateButtonElement();

    // If the fetch fails, it will return empty language lists. In this case,
    // the localStorageProxy will not have stored anything.
    assertEquals(testLanguageBrowserProxy.getStoredLocale(), '');
    assertEquals(testLanguageBrowserProxy.getStoredSourceLanguages().length, 0);
    assertEquals(testLanguageBrowserProxy.getStoredTargetLanguages().length, 0);

    const clientLanguageNames =
        (await testLanguageBrowserProxy.getClientLanguageList())
            .map((lang: Language) => lang.name);

    // Check the language picker list as it should be using the client language
    // list since a server language list could not be fetched.
    const sourceLanguageMenuItems =
        Array.from(overlayTranslateButtonElement.$.allSourceLanguagesMenu
                       .querySelectorAll<CrButtonElement>(
                           'cr-button:not(#sourceAutoDetectButton)'));
    assertTrue(sourceLanguageMenuItems !== null);
    assertTrue(sourceLanguageMenuItems.length > 0);
    sourceLanguageMenuItems.map((button: CrButtonElement) => {
      assertTrue(clientLanguageNames.includes(button.innerText.trim()));
    });

    const targetLanguageMenuItems =
        Array.from(overlayTranslateButtonElement.$.allTargetLanguagesMenu
                       .querySelectorAll<CrButtonElement>('cr-button'));
    assertTrue(targetLanguageMenuItems !== null);
    assertTrue(targetLanguageMenuItems.length > 0);
    targetLanguageMenuItems.map((button: CrButtonElement) => {
      assertTrue(clientLanguageNames.includes(button.innerText.trim()));
    });

    assertEquals(
        1, testBrowserProxy.handler.getCallCount('fetchSupportedLanguages'));
  });

  test('CacheTimeoutCausesFetchLanguageCall', async () => {
    loadTimeData.overrideValues({'languagesCacheTimeout': 1});
    // Do a fake store to test if languages are changed due to timeout.
    testLanguageBrowserProxy.storeLanguages(
        'en-US', TEST_FETCH_LANGUAGES, TEST_FETCH_LANGUAGES);
    testBrowserProxy.handler.setLanguagesToFetchForTesting(
        'en-US', TEST_FETCH_LANGUAGES_OTHER, TEST_FETCH_LANGUAGES_OTHER);
    await new Promise(
        resolve => setTimeout(resolve, 2));  // Advance time by 2ms for testing.
    await addTranslateButtonElement();

    // The local storage proxy should have stored the newly fetched languages.
    assertEquals(testLanguageBrowserProxy.getStoredLocale(), 'en-US');
    assertDeepEquals(
        testLanguageBrowserProxy.getStoredSourceLanguages(),
        TEST_FETCH_LANGUAGES_OTHER);
    assertDeepEquals(
        testLanguageBrowserProxy.getStoredTargetLanguages(),
        TEST_FETCH_LANGUAGES_OTHER);

    const oldLanguageNames =
        TEST_FETCH_LANGUAGES.map((lang: Language) => lang.name);
    const newLanguageNames =
        TEST_FETCH_LANGUAGES_OTHER.map((lang: Language) => lang.name);

    // Check the language picker list as it should be using the server language
    // list.
    const sourceLanguageMenuItems =
        Array.from(overlayTranslateButtonElement.$.allSourceLanguagesMenu
                       .querySelectorAll<CrButtonElement>(
                           'cr-button:not(#sourceAutoDetectButton)'));
    assertTrue(sourceLanguageMenuItems !== null);
    assertTrue(sourceLanguageMenuItems.length > 0);
    sourceLanguageMenuItems.map((button: CrButtonElement) => {
      assertTrue(newLanguageNames.includes(button.innerText.trim()));
      assertFalse(oldLanguageNames.includes(button.innerText.trim()));
    });

    const targetLanguageMenuItems =
        Array.from(overlayTranslateButtonElement.$.allTargetLanguagesMenu
                       .querySelectorAll<CrButtonElement>('cr-button'));
    assertTrue(targetLanguageMenuItems !== null);
    assertTrue(targetLanguageMenuItems.length > 0);
    targetLanguageMenuItems.map((button: CrButtonElement) => {
      assertTrue(newLanguageNames.includes(button.innerText.trim()));
      assertFalse(oldLanguageNames.includes(button.innerText.trim()));
    });

    assertEquals(
        1, testBrowserProxy.handler.getCallCount('fetchSupportedLanguages'));
  });

  test('StoreLastUsedSourceLanguage', async () => {
    testBrowserProxy.handler.setLanguagesToFetchForTesting(
        'en-US', TEST_FETCH_LANGUAGES, TEST_FETCH_LANGUAGES);
    await addTranslateButtonElement();

    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageButton));

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateEnableButton.click();

    // The source language button should be visible but the language picker
    // menu should not be visible.
    assertTrue(isVisible(overlayTranslateButtonElement.$.sourceLanguageButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));

    // Clicking the source language button should open the picker menu.
    overlayTranslateButtonElement.$.sourceLanguageButton.click();

    // The source language picker menu is visible.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));
    assertTrue(testLanguageBrowserProxy.getLastUsedSourceLanguage() === null);

    // Get a menu item button from the source language picker menu to click.
    const sourceLanguageMenuItem =
        overlayTranslateButtonElement.$.allSourceLanguagesMenu
            .querySelector<CrButtonElement>(
                'cr-button:not(#sourceAutoDetectButton)');
    assertTrue(sourceLanguageMenuItem !== null);
    sourceLanguageMenuItem.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // The source language button should be updated with the text of the new
    // source language.
    assertEquals(
        overlayTranslateButtonElement.$.sourceLanguageButton.innerText,
        sourceLanguageMenuItem.innerText.trim());
    assertEquals(testLanguageBrowserProxy.getLastUsedSourceLanguage(), 'es');

    overlayTranslateButtonElement.$.sourceAutoDetectButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    assertEquals(
        overlayTranslateButtonElement.$.sourceLanguageButton.innerText,
        loadTimeData.getString('detectLanguage'));
    assertTrue(testLanguageBrowserProxy.getLastUsedSourceLanguage() === null);
  });

  test('StoreLastUsedTargetLanguage', async () => {
    testBrowserProxy.handler.setLanguagesToFetchForTesting(
        'en-US', TEST_FETCH_LANGUAGES, TEST_FETCH_LANGUAGES);
    await addTranslateButtonElement();

    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageButton));

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateEnableButton.click();

    // The target language button should be visible but the language picker
    // menu should not be visible.
    assertTrue(isVisible(overlayTranslateButtonElement.$.targetLanguageButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguagePickerMenu));

    // Clicking the target language button should open the picker menu.
    overlayTranslateButtonElement.$.targetLanguageButton.click();

    // The target language picker menu is visible.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.targetLanguagePickerMenu));
    assertTrue(testLanguageBrowserProxy.getLastUsedTargetLanguage() === null);

    // Get a menu item button from the target language picker menu to click.
    const targetLanguageMenuItem =
        overlayTranslateButtonElement.$.allTargetLanguagesMenu
            .querySelector<CrButtonElement>('cr-button');
    assertTrue(targetLanguageMenuItem !== null);
    targetLanguageMenuItem.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // The target language button should be updated with the text of the new
    // target language.
    assertEquals(
        overlayTranslateButtonElement.$.targetLanguageButton.innerText,
        targetLanguageMenuItem.innerText.trim());
    assertEquals(testLanguageBrowserProxy.getLastUsedTargetLanguage(), 'es');
  });

  test('LocaleChangeCausesFetchLanguageCall', async () => {
    loadTimeData.overrideValues({'language': 'fr'});
    // Do a fake store to test if languages are changed due to locale change.
    testLanguageBrowserProxy.storeLanguages(
        'en-US', TEST_FETCH_LANGUAGES, TEST_FETCH_LANGUAGES);
    testBrowserProxy.handler.setLanguagesToFetchForTesting(
        'fr', TEST_FETCH_LANGUAGES_OTHER, TEST_FETCH_LANGUAGES_OTHER);
    await addTranslateButtonElement();

    // The local storage proxy should have stored the newly fetched languages.
    assertEquals(testLanguageBrowserProxy.getStoredLocale(), 'fr');
    assertDeepEquals(
        testLanguageBrowserProxy.getStoredSourceLanguages(),
        TEST_FETCH_LANGUAGES_OTHER);
    assertDeepEquals(
        testLanguageBrowserProxy.getStoredTargetLanguages(),
        TEST_FETCH_LANGUAGES_OTHER);

    const oldLanguageNames =
        TEST_FETCH_LANGUAGES.map((lang: Language) => lang.name);
    const newLanguageNames =
        TEST_FETCH_LANGUAGES_OTHER.map((lang: Language) => lang.name);

    // Check the language picker list as it should be using the server language
    // list.
    const sourceLanguageMenuItems =
        Array.from(overlayTranslateButtonElement.$.sourceLanguagePickerMenu
                       .querySelectorAll<CrButtonElement>(
                           'cr-button:not(#sourceAutoDetectButton)'));
    assertTrue(sourceLanguageMenuItems !== null);
    assertTrue(sourceLanguageMenuItems.length > 0);
    sourceLanguageMenuItems.map((button: CrButtonElement) => {
      assertTrue(newLanguageNames.includes(button.innerText.trim()));
      assertFalse(oldLanguageNames.includes(button.innerText.trim()));
    });

    const targetLanguageMenuItems =
        Array.from(overlayTranslateButtonElement.$.targetLanguagePickerMenu
                       .querySelectorAll<CrButtonElement>('cr-button'));
    assertTrue(targetLanguageMenuItems !== null);
    assertTrue(targetLanguageMenuItems.length > 0);
    targetLanguageMenuItems.map((button: CrButtonElement) => {
      assertTrue(newLanguageNames.includes(button.innerText.trim()));
      assertFalse(oldLanguageNames.includes(button.innerText.trim()));
    });

    assertEquals(
        1, testBrowserProxy.handler.getCallCount('fetchSupportedLanguages'));
  });

  test('RecentSourceLanguageClick', async () => {
    loadTimeData.overrideValues({'recentLanguagesAmount': 2});
    const storedRecentLanguageCode = TEST_FETCH_LANGUAGES[1]!.languageCode;
    testLanguageBrowserProxy.storeRecentSourceLanguages(
        [storedRecentLanguageCode]);
    testBrowserProxy.handler.setLanguagesToFetchForTesting(
        'en-US', TEST_FETCH_LANGUAGES, TEST_FETCH_LANGUAGES);
    await addTranslateButtonElement();
    assertTrue(
        testLanguageBrowserProxy.getRecentSourceLanguages().length === 1);

    const recentSourceLanguageMenuItem =
        overlayTranslateButtonElement.$.recentSourceLanguagesSection
            .querySelector<CrButtonElement>('cr-button');
    assertTrue(recentSourceLanguageMenuItem !== null);
    assertNotEquals(
        recentSourceLanguageMenuItem.innerText.trim(),
        overlayTranslateButtonElement.$.sourceLanguageButton.innerText);

    // Click on the recent language menu item.
    recentSourceLanguageMenuItem.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // Recent language list should not be added to.
    assertTrue(
        testLanguageBrowserProxy.getRecentSourceLanguages().length === 1);
    assertEquals(
        recentSourceLanguageMenuItem.innerText.trim(),
        overlayTranslateButtonElement.$.sourceLanguageButton.innerText);
  });

  test('RecentTargetLanguageClick', async () => {
    loadTimeData.overrideValues({'recentLanguagesAmount': 2});
    const storedRecentLanguageCode = TEST_FETCH_LANGUAGES[1]!.languageCode;
    testLanguageBrowserProxy.storeRecentTargetLanguages(
        [storedRecentLanguageCode]);
    testBrowserProxy.handler.setLanguagesToFetchForTesting(
        'en-US', TEST_FETCH_LANGUAGES, TEST_FETCH_LANGUAGES);
    await addTranslateButtonElement();
    assertTrue(
        testLanguageBrowserProxy.getRecentTargetLanguages().length === 1);

    const recentTargetLanguageMenuItem =
        overlayTranslateButtonElement.$.recentTargetLanguagesSection
            .querySelector<CrButtonElement>('cr-button');
    assertTrue(recentTargetLanguageMenuItem !== null);
    assertNotEquals(
        recentTargetLanguageMenuItem.innerText.trim(),
        overlayTranslateButtonElement.$.targetLanguageButton.innerText);

    // Click on the recent language menu item.
    recentTargetLanguageMenuItem.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // Recent language list should not be added to.
    assertTrue(
        testLanguageBrowserProxy.getRecentTargetLanguages().length === 1);
    assertEquals(
        recentTargetLanguageMenuItem.innerText.trim(),
        overlayTranslateButtonElement.$.targetLanguageButton.innerText);
  });

  test('AutoDetectLanguageNotAddedToRecents', async () => {
    loadTimeData.overrideValues({'recentLanguagesAmount': 2});
    testBrowserProxy.handler.setLanguagesToFetchForTesting(
        'en-US', TEST_FETCH_LANGUAGES, TEST_FETCH_LANGUAGES);
    await addTranslateButtonElement();
    assertTrue(
        testLanguageBrowserProxy.getRecentSourceLanguages().length === 0);

    // Get a menu item button from the source language picker menu to click.
    const sourceLanguageMenuItem =
        overlayTranslateButtonElement.$.allSourceLanguagesMenu
            .querySelector<CrButtonElement>(
                'cr-button:not(#sourceAutoDetectButton)');
    assertTrue(sourceLanguageMenuItem !== null);
    sourceLanguageMenuItem.click();
    const sourceLanguage =
        overlayTranslateButtonElement.$.sourceLanguagePickerContainer
            .itemForElement(sourceLanguageMenuItem);
    await waitAfterNextRender(overlayTranslateButtonElement);

    // The source language button should be updated with the text of the new
    // source language.
    assertEquals(
        overlayTranslateButtonElement.$.sourceLanguageButton.innerText,
        sourceLanguageMenuItem.innerText.trim());
    let recentLanguages = testLanguageBrowserProxy.getRecentSourceLanguages();
    const firstRecentLanguage = recentLanguages[0]!;
    assertTrue(recentLanguages.length === 1);
    assertEquals(firstRecentLanguage, sourceLanguage.languageCode);

    overlayTranslateButtonElement.$.sourceAutoDetectButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    assertEquals(
        overlayTranslateButtonElement.$.sourceLanguageButton.innerText,
        loadTimeData.getString('detectLanguage'));
    assertTrue(recentLanguages.length === 1);
    recentLanguages = testLanguageBrowserProxy.getRecentSourceLanguages();
    const secondRecentLanguage = recentLanguages[0]!;
    assertTrue(secondRecentLanguage === firstRecentLanguage);
    assertNotEquals(secondRecentLanguage, 'auto');
  });

  test('StoreRecentSourceLanguages', async () => {
    testBrowserProxy.handler.setLanguagesToFetchForTesting(
        'en-US', TEST_FETCH_LANGUAGES, TEST_FETCH_LANGUAGES);
    await addTranslateButtonElement();
    assertTrue(
        testLanguageBrowserProxy.getRecentSourceLanguages().length === 0);

    // Get a menu item button from the source language picker menu to click.
    const sourceLanguageMenuItem =
        overlayTranslateButtonElement.$.allSourceLanguagesMenu
            .querySelector<CrButtonElement>(
                'cr-button:not(#sourceAutoDetectButton)');
    assertTrue(sourceLanguageMenuItem !== null);
    sourceLanguageMenuItem.click();
    const sourceLanguage =
        overlayTranslateButtonElement.$.sourceLanguagePickerContainer
            .itemForElement(sourceLanguageMenuItem);
    await waitAfterNextRender(overlayTranslateButtonElement);

    // The source language button should be updated with the text of the new
    // source language.
    assertEquals(
        overlayTranslateButtonElement.$.sourceLanguageButton.innerText,
        sourceLanguageMenuItem.innerText.trim());
    let recentLanguages = testLanguageBrowserProxy.getRecentSourceLanguages();
    const firstRecentLanguage = recentLanguages[0]!;
    assertTrue(recentLanguages.length === 1);
    assertEquals(firstRecentLanguage, sourceLanguage.languageCode);

    // Make sure the recent languages do not exceed the max.
    loadTimeData.overrideValues({'recentLanguagesAmount': 1});
    const sourceLanguageMenuItems =
        overlayTranslateButtonElement.$.allSourceLanguagesMenu
            .querySelectorAll<CrButtonElement>(
                'cr-button:not(#sourceAutoDetectButton)');
    const filteredMenuItems =
        Array.from(sourceLanguageMenuItems).filter((item: CrButtonElement) => {
          return item.innerText.trim() !==
              sourceLanguageMenuItem.innerText.trim();
        });
    assertTrue(filteredMenuItems.length > 0);
    const otherSourceLanguageMenuItem = filteredMenuItems[0] as CrButtonElement;
    const secondSourceLanguage =
        overlayTranslateButtonElement.$.sourceLanguagePickerContainer
            .itemForElement(otherSourceLanguageMenuItem);
    assertTrue(otherSourceLanguageMenuItem !== null);
    assertTrue(
        otherSourceLanguageMenuItem.innerText.trim() !==
        sourceLanguageMenuItem.innerText.trim());
    otherSourceLanguageMenuItem.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    recentLanguages = testLanguageBrowserProxy.getRecentSourceLanguages();
    const secondRecentLanguage = recentLanguages[0]!;
    assertTrue(recentLanguages.length === 1);
    assertFalse(secondRecentLanguage === firstRecentLanguage);
    assertEquals(secondRecentLanguage, secondSourceLanguage.languageCode);
  });

  test('StoreRecentTargetLanguages', async () => {
    testBrowserProxy.handler.setLanguagesToFetchForTesting(
        'en-US', TEST_FETCH_LANGUAGES, TEST_FETCH_LANGUAGES);
    await addTranslateButtonElement();
    assertTrue(
        testLanguageBrowserProxy.getRecentTargetLanguages().length === 0);

    // Get a menu item button from the target language picker menu to click.
    const targetLanguageMenuItems =
        overlayTranslateButtonElement.$.allTargetLanguagesMenu
            .querySelectorAll<CrButtonElement>('cr-button');
    assertTrue(targetLanguageMenuItems !== null);
    const filteredMenuItems =
        Array.from(targetLanguageMenuItems).filter((item: CrButtonElement) => {
          return item.innerText.trim() !==
              overlayTranslateButtonElement.$.targetLanguageButton.innerText;
        });
    assertTrue(filteredMenuItems.length > 0);
    const targetLanguageMenuItem = filteredMenuItems[0] as CrButtonElement;
    assertTrue(targetLanguageMenuItem !== null);
    const targetLanguage =
        overlayTranslateButtonElement.$.targetLanguagePickerContainer
            .itemForElement(targetLanguageMenuItem);
    targetLanguageMenuItem.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // The target language button should be updated with the text of the new
    // target language.
    assertEquals(
        overlayTranslateButtonElement.$.targetLanguageButton.innerText,
        targetLanguageMenuItem.innerText.trim());
    let recentLanguages = testLanguageBrowserProxy.getRecentTargetLanguages();
    const firstRecentLanguage = recentLanguages[0]!;
    assertTrue(recentLanguages.length === 1);
    assertEquals(firstRecentLanguage, targetLanguage.languageCode);

    // Make sure the recent languages do not exceed the max.
    loadTimeData.overrideValues({'recentLanguagesAmount': 1});
    const newFilteredMenuItems =
        Array.from(targetLanguageMenuItems).filter((item: CrButtonElement) => {
          return item.innerText.trim() !==
              targetLanguageMenuItem.innerText.trim();
        });
    assertTrue(newFilteredMenuItems.length > 0);
    const otherTargetLanguageMenuItem =
        newFilteredMenuItems[0] as CrButtonElement;
    assertTrue(otherTargetLanguageMenuItem !== null);
    const secondTargetLanguage =
        overlayTranslateButtonElement.$.targetLanguagePickerContainer
            .itemForElement(otherTargetLanguageMenuItem);
    assertTrue(
        otherTargetLanguageMenuItem.innerText.trim() !==
        targetLanguageMenuItem.innerText.trim());
    otherTargetLanguageMenuItem.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    recentLanguages = testLanguageBrowserProxy.getRecentTargetLanguages();
    const secondRecentLanguage = recentLanguages[0]!;
    assertTrue(recentLanguages.length === 1);
    assertFalse(secondRecentLanguage === firstRecentLanguage);
    assertEquals(secondRecentLanguage, secondTargetLanguage.languageCode);
  });

  test('SearchAndClickSourceLanguages', async () => {
    // Store a recent language so we can test visibility of the recent language
    // section.
    const storedRecentLanguageCode = TEST_FETCH_LANGUAGES[1]!.languageCode;
    testLanguageBrowserProxy.storeRecentSourceLanguages(
        [storedRecentLanguageCode]);

    testBrowserProxy.handler.setLanguagesToFetchForTesting(
        'en-US', TEST_FETCH_LANGUAGES, TEST_FETCH_LANGUAGES);
    await addTranslateButtonElement();
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageButton));

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateEnableButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // The source language button should be visible but the language picker
    // menu should not be visible.
    assertTrue(isVisible(overlayTranslateButtonElement.$.sourceLanguageButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));

    // Clicking the source language button should open the picker menu.
    overlayTranslateButtonElement.$.sourceLanguageButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // The source language picker menu is visible as well as the search button.
    // The searchbox should not be.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageSearchButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageSearchbox));

    // Clicking the search button should make the search box visible.
    overlayTranslateButtonElement.$.sourceLanguageSearchButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageSearchButton));
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageSearchbox));

    // All and recent language sections should still be present until the user
    // types into the searchbox. The searched language section is also not
    // visible.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.allSourceLanguagesMenu));
    assertTrue(isVisible(
        overlayTranslateButtonElement.$.recentSourceLanguagesSection));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.searchSourceLanguagePicker));

    dispatchInputEvent(
        overlayTranslateButtonElement.$.sourceLanguageSearchbox, 's');
    await waitAfterNextRender(overlayTranslateButtonElement);

    // The all and recent languages sections should no longer be visible, while
    // the searched language section should now be visible.
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.allSourceLanguagesMenu));
    assertFalse(isVisible(
        overlayTranslateButtonElement.$.recentSourceLanguagesSection));
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.searchSourceLanguagePicker));

    // There should only be one visible language in the searched language picker
    // section.
    const sourceLanguageMenuItems =
        overlayTranslateButtonElement.$.searchSourceLanguagePicker
            .querySelectorAll<CrButtonElement>('cr-button');
    assertTrue(sourceLanguageMenuItems.length === 1);
    const menuItem = sourceLanguageMenuItems[0]!;
    // Reset proxy so we only get the latest call.
    testBrowserProxy.handler.reset();
    menuItem.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // Assert everything has closed in the picker.
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));

    // The source language button should update.
    assertEquals(
        overlayTranslateButtonElement.$.sourceLanguageButton.innerText,
        menuItem.innerText.trim());
    const args = await testBrowserProxy.handler.whenCalled(
        'issueTranslateFullPageRequest');
    const sourceLanguage = args[0];
    assertEquals(sourceLanguage, 'es');
    // Verify that a source languages changes were recorded in this test.
    assertEquals(
        1,
        metrics.count(
            'Lens.Overlay.Overlay.UserAction',
            UserAction.kTranslateSourceLanguageChanged));
    assertEquals(
        1,
        metrics.count(
            'Lens.Overlay.Overlay.ByInvocationSource.AppMenu.UserAction',
            UserAction.kTranslateSourceLanguageChanged));

    // Verify that re-opening the language picker menu has reset the searchbox
    // state.
    overlayTranslateButtonElement.$.sourceLanguageButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // The source language picker menu is visible as well as the search button.
    // The searchbox should not be.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageSearchButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageSearchbox));

    // All and recent language sections should still be present until the user
    // types into the searchbox. The searched language section is also not
    // visible.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.allSourceLanguagesMenu));
    assertTrue(isVisible(
        overlayTranslateButtonElement.$.recentSourceLanguagesSection));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.searchSourceLanguagePicker));
  });

  test('SearchAndClickTargetLanguages', async () => {
    // Store a recent language so we can test visibility of the recent language
    // section.
    const storedRecentLanguageCode = TEST_FETCH_LANGUAGES[1]!.languageCode;
    testLanguageBrowserProxy.storeRecentTargetLanguages(
        [storedRecentLanguageCode]);

    testBrowserProxy.handler.setLanguagesToFetchForTesting(
        'en-US', TEST_FETCH_LANGUAGES, TEST_FETCH_LANGUAGES);
    await addTranslateButtonElement();
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguageButton));

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateEnableButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // The target language button should be visible but the language picker
    // menu should not be visible.
    assertTrue(isVisible(overlayTranslateButtonElement.$.targetLanguageButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguagePickerMenu));

    // Clicking the target language button should open the picker menu.
    overlayTranslateButtonElement.$.targetLanguageButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // The target language picker menu is visible as well as the search button.
    // The searchbox should not be.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.targetLanguagePickerMenu));
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.targetLanguageSearchButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguageSearchbox));

    // Clicking the search button should make the search box visible.
    overlayTranslateButtonElement.$.targetLanguageSearchButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguageSearchButton));
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.targetLanguageSearchbox));

    // All and recent language sections should still be present until the user
    // types into the searchbox. The searched language section is also not
    // visible.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.allTargetLanguagesMenu));
    assertTrue(isVisible(
        overlayTranslateButtonElement.$.recentTargetLanguagesSection));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.searchTargetLanguagePicker));

    dispatchInputEvent(
        overlayTranslateButtonElement.$.targetLanguageSearchbox, 's');
    await waitAfterNextRender(overlayTranslateButtonElement);

    // The all and recent languages sections should no longer be visible, while
    // the searched language section should now be visible.
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.allTargetLanguagesMenu));
    assertFalse(isVisible(
        overlayTranslateButtonElement.$.recentTargetLanguagesSection));
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.searchTargetLanguagePicker));

    // There should only be one visible language in the searched language picker
    // section.
    const targetLanguageMenuItems =
        overlayTranslateButtonElement.$.searchTargetLanguagePicker
            .querySelectorAll<CrButtonElement>('cr-button');
    assertTrue(targetLanguageMenuItems.length === 1);
    const menuItem = targetLanguageMenuItems[0]!;
    // Reset proxy so we only get the latest call.
    testBrowserProxy.handler.reset();
    menuItem.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // Assert everything has closed in the picker.
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguagePickerMenu));

    // The target language button should update.
    assertEquals(
        overlayTranslateButtonElement.$.targetLanguageButton.innerText,
        menuItem.innerText.trim());
    const args = await testBrowserProxy.handler.whenCalled(
        'issueTranslateFullPageRequest');
    const targetLanguage = args[1];
    assertEquals(targetLanguage, 'es');
    // Verify that a target languages changes were recorded in this test.
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

    // Verify that re-opening the language picker menu has reset the searchbox
    // state.
    overlayTranslateButtonElement.$.targetLanguageButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // The target language picker menu is visible as well as the search button.
    // The searchbox should not be.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.targetLanguagePickerMenu));
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.targetLanguageSearchButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguageSearchbox));

    // All and recent language sections should still be present until the user
    // types into the searchbox. The searched language section is also not
    // visible.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.allTargetLanguagesMenu));
    assertTrue(isVisible(
        overlayTranslateButtonElement.$.recentTargetLanguagesSection));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.searchTargetLanguagePicker));
  });

  test('BackButtonWhenSearchingSourceLanguages', async () => {
    // Store a recent language so we can test visibility of the recent language
    // section.
    const storedRecentLanguageCode = TEST_FETCH_LANGUAGES[1]!.languageCode;
    testLanguageBrowserProxy.storeRecentSourceLanguages(
        [storedRecentLanguageCode]);

    testBrowserProxy.handler.setLanguagesToFetchForTesting(
        'en-US', TEST_FETCH_LANGUAGES, TEST_FETCH_LANGUAGES);
    await addTranslateButtonElement();
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageButton));

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateEnableButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // The source language button should be visible but the language picker
    // menu should not be visible.
    assertTrue(isVisible(overlayTranslateButtonElement.$.sourceLanguageButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));

    // Clicking the source language button should open the picker menu.
    overlayTranslateButtonElement.$.sourceLanguageButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // The source language picker menu is visible as well as the search button.
    // The searchbox should not be.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageSearchButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageSearchbox));

    // Clicking the search button should make the search box visible.
    overlayTranslateButtonElement.$.sourceLanguageSearchButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageSearchButton));
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageSearchbox));

    // All and recent language sections should still be present until the user
    // types into the searchbox. The searched language section is also not
    // visible.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.allSourceLanguagesMenu));
    assertTrue(isVisible(
        overlayTranslateButtonElement.$.recentSourceLanguagesSection));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.searchSourceLanguagePicker));

    // Clicking the back button SHOULD NOT close the language picker menu in
    // this case.
    overlayTranslateButtonElement.$.sourceLanguagePickerBackButton.click();

    // The search button should re-appear and the searchbox should hide.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageSearchButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageSearchbox));

    // All and recent language sections should still be present until the user
    // types into the searchbox. The searched language section is also not
    // visible.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.allSourceLanguagesMenu));
    assertTrue(isVisible(
        overlayTranslateButtonElement.$.recentSourceLanguagesSection));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.searchSourceLanguagePicker));
  });

  test('BackButtonWhenSearchingTargetLanguages', async () => {
    // Store a recent language so we can test visibility of the recent language
    // section.
    const storedRecentLanguageCode = TEST_FETCH_LANGUAGES[1]!.languageCode;
    testLanguageBrowserProxy.storeRecentTargetLanguages(
        [storedRecentLanguageCode]);

    testBrowserProxy.handler.setLanguagesToFetchForTesting(
        'en-US', TEST_FETCH_LANGUAGES, TEST_FETCH_LANGUAGES);
    await addTranslateButtonElement();
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguageButton));

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateEnableButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // The target language button should be visible but the language picker
    // menu should not be visible.
    assertTrue(isVisible(overlayTranslateButtonElement.$.targetLanguageButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguagePickerMenu));

    // Clicking the target language button should open the picker menu.
    overlayTranslateButtonElement.$.targetLanguageButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);

    // The target language picker menu is visible as well as the search button.
    // The searchbox should not be.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.targetLanguagePickerMenu));
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.targetLanguageSearchButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguageSearchbox));

    // Clicking the search button should make the search box visible.
    overlayTranslateButtonElement.$.targetLanguageSearchButton.click();
    await waitAfterNextRender(overlayTranslateButtonElement);
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguageSearchButton));
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.targetLanguageSearchbox));

    // All and recent language sections should still be present until the user
    // types into the searchbox. The searched language section is also not
    // visible.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.allTargetLanguagesMenu));
    assertTrue(isVisible(
        overlayTranslateButtonElement.$.recentTargetLanguagesSection));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.searchTargetLanguagePicker));

    // Clicking the back button SHOULD NOT close the language picker menu in
    // this case.
    overlayTranslateButtonElement.$.targetLanguagePickerBackButton.click();

    // The search button should re-appear and the searchbox should hide.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.targetLanguageSearchButton));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguageSearchbox));

    // All and recent language sections should still be present until the user
    // types into the searchbox. The searched language section is also not
    // visible.
    assertTrue(
        isVisible(overlayTranslateButtonElement.$.allTargetLanguagesMenu));
    assertTrue(isVisible(
        overlayTranslateButtonElement.$.recentTargetLanguagesSection));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.searchTargetLanguagePicker));
  });

  test('SourceLanguageFocusedWithKeyPress', async () => {
    // Need to explicitly set this to false since when enabled a key press
    // should open the searchbox instead of focusing the language.
    loadTimeData.overrideValues({
      'shouldFetchSupportedLanguages': false,
    });
    await addTranslateButtonElement();

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


    const sourceLanguageMenuItems =
        overlayTranslateButtonElement.$.allSourceLanguagesMenu
            .querySelectorAll<CrButtonElement>('cr-button');
    const swahiliMenuItem: CrButtonElement =
        Array.from(sourceLanguageMenuItems).filter((item: CrButtonElement) => {
          return item.innerText === 'Swahili';
        })[0] as CrButtonElement;
    assertTrue(swahiliMenuItem !== undefined);
    assertNotEquals(
        swahiliMenuItem,
        overlayTranslateButtonElement.shadowRoot!.activeElement);

    // Get a menu item button from the source language picker menu.
    const keyDownEvent = new KeyboardEvent('keydown', {
      key: 's',
      code: 'KeyS',
    });
    overlayTranslateButtonElement.$.sourceLanguagePickerMenu.dispatchEvent(
        keyDownEvent);

    assertTrue(isVisible(swahiliMenuItem));
    assertEquals(
        swahiliMenuItem,
        overlayTranslateButtonElement.shadowRoot!.activeElement);
  });

  test('TargetLanguageFocusedWithKeyPress', async () => {
    // Need to explicitly set this to false since when enabled a key press
    // should open the searchbox instead of focusing the language.
    loadTimeData.overrideValues({
      'shouldFetchSupportedLanguages': false,
    });
    await addTranslateButtonElement();

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

    const targetLanguageMenuItems =
        overlayTranslateButtonElement.$.allTargetLanguagesMenu
            .querySelectorAll<CrButtonElement>('cr-button');
    const swahiliMenuItem: CrButtonElement =
        Array.from(targetLanguageMenuItems).filter((item: CrButtonElement) => {
          return item.innerText === 'Swahili';
        })[0] as CrButtonElement;
    assertTrue(swahiliMenuItem !== undefined);
    assertNotEquals(
        swahiliMenuItem,
        overlayTranslateButtonElement.shadowRoot!.activeElement);

    // Get a menu item button from the target language picker menu.
    const keyDownEvent = new KeyboardEvent('keydown', {
      key: 's',
      code: 'KeyS',
    });
    overlayTranslateButtonElement.$.targetLanguagePickerMenu.dispatchEvent(
        keyDownEvent);

    assertTrue(isVisible(swahiliMenuItem));
    assertEquals(
        swahiliMenuItem,
        overlayTranslateButtonElement.shadowRoot!.activeElement);
  });
});
