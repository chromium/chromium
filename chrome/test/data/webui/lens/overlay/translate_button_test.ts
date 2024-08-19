// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/translate_button.js';

import {BrowserProxyImpl} from 'chrome-untrusted://lens/browser_proxy.js';
import {LanguageBrowserProxyImpl} from 'chrome-untrusted://lens/language_browser_proxy.js';
import type {TranslateButtonElement} from 'chrome-untrusted://lens/translate_button.js';
import type {CrButtonElement} from 'chrome-untrusted://resources/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {flushTasks} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';

import {TestLanguageBrowserProxy} from './test_language_browser_proxy.js';
import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

suite('OverlayTranslateButton', function() {
  let overlayTranslateButtonElement: TranslateButtonElement;
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let testLanguageBrowserProxy: TestLanguageBrowserProxy;

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
    await flushTasks();
  });

  test('TranslateButtonClick', async () => {
    assertFalse(isVisible(overlayTranslateButtonElement.$.languagePicker));

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateButton.click();

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
    assertTrue(isVisible(overlayTranslateButtonElement.$.languagePicker));

    // Clicking again should toggle the language picker but not send another
    // request.
    overlayTranslateButtonElement.$.translateButton.click();
    assertEquals(
        1,
        testBrowserProxy.handler.getCallCount('issueTranslateFullPageRequest'));

    // Language picker should be hidden again.
    assertFalse(isVisible(overlayTranslateButtonElement.$.languagePicker));
  });

  test('SourceLanguageButtonClick', () => {
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageButton));

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateButton.click();

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
    overlayTranslateButtonElement.$.translateButton.click();

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

  test('SourceLanguageMenuItemClick', () => {
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguageButton));

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateButton.click();

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
        sourceLanguageMenuItem.innerText);

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
    // text.
    overlayTranslateButtonElement.$.sourceAutoDetectButton.click();
    assertEquals(
        overlayTranslateButtonElement.$.sourceLanguageButton.innerText,
        loadTimeData.getString('autoDetect'));
  });

  test('TargetLanguageMenuItemClick', () => {
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguageButton));

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateButton.click();

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

    // The target language button should be updated with the text of the new
    // target language.
    assertEquals(
        overlayTranslateButtonElement.$.targetLanguageButton.innerText,
        targetLanguageMenuItem.innerText);

    // Both of the language picker menus should be hidden after this.
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.targetLanguagePickerMenu));
    assertFalse(
        isVisible(overlayTranslateButtonElement.$.sourceLanguagePickerMenu));
  });
});
