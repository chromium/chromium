// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PluginController, SaveRequestType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {createMockPdfPluginForTest, finishInkStroke, getAnnotationsBar} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const viewerToolbar = viewer.$.toolbar;
const controller = PluginController.getInstance();
const mockPlugin = createMockPdfPluginForTest();
controller.setPluginForTesting(mockPlugin);

function getDownloadControls() {
  const downloadControls =
      viewerToolbar.shadowRoot!.querySelector('viewer-download-controls');
  assert(downloadControls);
  return downloadControls;
}

chrome.test.runTests([
  // Tests that while in annotation mode, on a PDF without any edits, clicking
  // the download button will save the PDF as original.
  async function testSaveOriginal() {
    viewerToolbar.toggleAnnotation();
    chrome.test.assertTrue(viewerToolbar.annotationMode);

    const downloadControls = getDownloadControls();
    const downloadButton = downloadControls.$.download;
    const actionMenu = downloadControls.$.menu;
    chrome.test.assertFalse(actionMenu.open);

    downloadButton.click();

    // A message should be sent to the plugin to save as original.
    await eventToPromise('save', viewer);
    const saveMessage = mockPlugin.findMessage('save');
    chrome.test.assertTrue(saveMessage !== undefined);
    chrome.test.assertEq(saveMessage.saveRequestType, SaveRequestType.ORIGINAL);
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertTrue(viewerToolbar.annotationMode);
    chrome.test.succeed();
  },

  // Tests that while in annotation mode, after adding an ink stroke, clicking
  // the download button will prompt the download menu.
  async function testSaveMenuWithStroke() {
    mockPlugin.clearMessages();

    const downloadControls = getDownloadControls();
    const downloadButton = downloadControls.$.download;
    const actionMenu = downloadControls.$.menu;

    chrome.test.assertFalse(actionMenu.open);

    finishInkStroke(controller);
    downloadButton.click();

    // The download menu should be shown.
    await eventToPromise('download-menu-shown-for-testing', downloadControls);
    chrome.test.assertTrue(mockPlugin.findMessage('save') === undefined);
    chrome.test.assertTrue(actionMenu.open);
    chrome.test.succeed();
  },

  // Tests that while outside of annotation mode, on a PDF with an ink stroke,
  // clicking the download button will prompt the download menu.
  async function testSaveMenuWithStrokeExitAnnotationMode() {
    chrome.test.assertTrue(viewerToolbar.annotationMode);

    viewerToolbar.toggleAnnotation();
    chrome.test.assertFalse(viewerToolbar.annotationMode);

    const downloadControls = getDownloadControls();
    const downloadButton = downloadControls.$.download;
    const actionMenu = downloadControls.$.menu;
    actionMenu.close();

    downloadButton.click();

    // The download menu should be shown.
    await eventToPromise('download-menu-shown-for-testing', downloadControls);
    chrome.test.assertTrue(mockPlugin.findMessage('save') === undefined);
    chrome.test.assertTrue(actionMenu.open);
    chrome.test.succeed();
  },

  // An undo operation will remove the only ink stroke from the PDF. Tests that
  // while in annotation mode, after an undo operation, clicking the download
  // button will save the PDF as original.
  async function testSaveOriginalAfterUndo() {
    chrome.test.assertFalse(viewerToolbar.annotationMode);

    viewerToolbar.toggleAnnotation();
    chrome.test.assertTrue(viewerToolbar.annotationMode);

    mockPlugin.clearMessages();

    const annotationBar = getAnnotationsBar(viewerToolbar);
    const undoButton = annotationBar.$.undo;
    chrome.test.assertFalse(undoButton.disabled);

    const downloadControls = getDownloadControls();
    const downloadButton = downloadControls.$.download;
    const actionMenu = downloadControls.$.menu;
    actionMenu.close();

    undoButton.click();

    // After undo, there aren't any ink strokes on the PDF, and the button will
    // be disabled.
    chrome.test.assertTrue(undoButton.disabled);

    downloadButton.click();

    // A message should be sent to the plugin to save as original.
    await eventToPromise('save', viewer);
    const saveMessage = mockPlugin.findMessage('save');
    chrome.test.assertTrue(saveMessage !== undefined);
    chrome.test.assertEq(saveMessage.saveRequestType, SaveRequestType.ORIGINAL);
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.succeed();
  },

  // A redo operation will add the ink stroke back to the PDF. Tests that while
  // in annotation mode, after a redo operation, clicking the download button
  // will prompt the download menu.
  async function testSaveMenuAfterRedo() {
    mockPlugin.clearMessages();

    const annotationBar = getAnnotationsBar(viewerToolbar);
    const redoButton = annotationBar.$.redo;
    chrome.test.assertFalse(redoButton.disabled);

    const downloadControls = getDownloadControls();
    const downloadButton = downloadControls.$.download;
    const actionMenu = downloadControls.$.menu;

    redoButton.click();
    downloadButton.click();

    // The download menu should be shown.
    await eventToPromise('download-menu-shown-for-testing', downloadControls);
    chrome.test.assertTrue(mockPlugin.findMessage('save') === undefined);
    chrome.test.assertTrue(actionMenu.open);
    chrome.test.succeed();
  },

  // Tests that while in annotation mode, after undoing all edits on the PDF,
  // clicking the download button will save the PDF as original.
  async function testSaveOriginalAfterFullUndo() {
    mockPlugin.clearMessages();

    // Add another ink stroke. There should now be two ink strokes on the PDF.
    finishInkStroke(controller);

    const annotationBar = getAnnotationsBar(viewerToolbar);
    const undoButton = annotationBar.$.undo;
    chrome.test.assertFalse(undoButton.disabled);

    // Undo both ink strokes.
    undoButton.click();
    undoButton.click();

    // There shouldn't be any ink strokes on the PDF.
    chrome.test.assertTrue(undoButton.disabled);

    const downloadControls = getDownloadControls();
    const downloadButton = downloadControls.$.download;
    const actionMenu = downloadControls.$.menu;
    actionMenu.close();

    downloadButton.click();

    // A message should be sent to the plugin to save as original.
    await eventToPromise('save', viewer);
    const saveMessage = mockPlugin.findMessage('save');
    chrome.test.assertTrue(saveMessage !== undefined);
    chrome.test.assertEq(saveMessage.saveRequestType, SaveRequestType.ORIGINAL);
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.succeed();
  },
]);
