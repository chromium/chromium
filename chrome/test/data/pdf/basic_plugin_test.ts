// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PdfScriptingApi} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_scripting_api.js';
import type {PdfViewerElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {PluginController} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import type {ModifiersParam} from 'chrome://webui-test/keyboard_mock_interactions.js';

import {checkPdfTitleIsExpectedTitle} from './test_util.js';

function getSelectedText(client: PdfScriptingApi): Promise<string> {
  return new Promise((resolve) => client.getSelectedText(resolve));
}

async function checkSelectedTextIsExpectedText(client: PdfScriptingApi):
    Promise<void> {
  const selectedText = await getSelectedText(client);
  chrome.test.assertEq('this is some text\nsome more text', selectedText);
}

async function checkNoSelectedText(client: PdfScriptingApi): Promise<void> {
  const selectedText = await getSelectedText(client);
  chrome.test.assertEq(0, selectedText.length);
}

function resetTextSelection() {
  // Toggling presentation mode has the side effect of clearing the text
  // selection. While this is somewhat hacky, it is reliable and removes the
  // need to add a dedicated call just for testing purposes.
  const pluginController = PluginController.getInstance();
  pluginController.setPresentationMode(true);
  pluginController.setPresentationMode(false);
}

/**
 * These tests require that the PDF plugin be available to run correctly.
 */
chrome.test.runTests([
  /**
   * Test that the page is sized to the size of the document.
   */
  function testPageSize() {
    const viewer = document.body.querySelector<PdfViewerElement>('#viewer')!;
    // Verify that the initial zoom is less than or equal to 100%.
    chrome.test.assertTrue(viewer.viewport.getZoom() <= 1);

    viewer.viewport.setZoom(1);
    chrome.test.assertEq(826, viewer.viewport.contentSize.width);
    chrome.test.assertEq(1066, viewer.viewport.contentSize.height);
    chrome.test.succeed();
  },

  async function testGetSelectedTextWithNoSelection() {
    resetTextSelection();

    const client = new PdfScriptingApi(window, window);
    await checkNoSelectedText(client);
    chrome.test.succeed();
  },

  async function testGetSelectedTextViaScriptingApi() {
    resetTextSelection();

    const client = new PdfScriptingApi(window, window);
    client.selectAll();
    await checkSelectedTextIsExpectedText(client);
    chrome.test.succeed();
  },

  async function testGetSelectedTextViaKeyPress() {
    resetTextSelection();

    // <if expr="is_macosx">
    const modifier = 'meta';
    // </if>
    // <if expr="not is_macosx">
    const modifier = 'ctrl';
    // </if>
    pressAndReleaseKeyOn(document.documentElement, 65, modifier, 'a');

    const client = new PdfScriptingApi(window, window);
    await checkSelectedTextIsExpectedText(client);
    chrome.test.succeed();
  },

  async function testGetSelectedTextViaInvalidKeyPresses() {
    resetTextSelection();

    const modifiers: ModifiersParam[] = ['shift', 'alt'];
    // <if expr="is_macosx">
    modifiers.push(['ctrl']);
    // </if>
    // <if expr="not is_macosx">
    modifiers.push(['meta']);
    // </if>
    modifiers.push(
        ['shift', 'ctrl'], ['shift', 'alt'], ['shift', 'meta'], ['ctrl', 'alt'],
        ['ctrl', 'meta'], ['alt', 'meta']);
    modifiers.push(
        ['shift', 'ctrl', 'alt'], ['shift', 'ctrl', 'meta'],
        ['shift', 'alt', 'meta'], ['ctrl', 'alt', 'meta']);
    modifiers.push(['shift', 'ctrl', 'alt', 'meta']);

    const client = new PdfScriptingApi(window, window);
    for (const mods of modifiers) {
      pressAndReleaseKeyOn(document.documentElement, 65, mods, 'a');
      await checkNoSelectedText(client);
    }
    chrome.test.succeed();
  },

  /**
   * Test that the filename is used as the title.pdf.
   */
  function testHasCorrectTitle() {
    chrome.test.assertTrue(checkPdfTitleIsExpectedTitle('test.pdf'));
    chrome.test.succeed();
  },
]);
