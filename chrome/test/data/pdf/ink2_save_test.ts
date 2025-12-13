// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationMode, PluginController, PluginControllerEventType, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createTextBox, getRequiredElement, setupMockMetricsPrivate, setupTestMockPluginForInk, startFinishModifiedInkStroke} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const viewerToolbar = viewer.$.toolbar;
const controller = PluginController.getInstance();
const mockPlugin = setupTestMockPluginForInk();
const mockMetricsPrivate = setupMockMetricsPrivate();
const SaveRequestType = chrome.pdfViewerPrivate.SaveRequestType;
type SaveRequestType = chrome.pdfViewerPrivate.SaveRequestType;

function getDownloadControls() {
  return getRequiredElement(viewerToolbar, 'viewer-download-controls');
}

function getFirstSaveMessageName(): string {
  return loadTimeData.getBoolean('pdfGetSaveDataInBlocks') ?
      'getSuggestedFileName' :
      'save';
}

function getSaveRequestType(message: any): SaveRequestType {
  return loadTimeData.getBoolean('pdfGetSaveDataInBlocks') ?
      message.saveRequestTypeForTesting :
      message.saveRequestType;
}

// Test saving with annotations. The download control's action menu should be
// opened.
async function testSaveWithAnnotations() {
  const downloadControls = getDownloadControls();
  const actionMenu = downloadControls.$.menu;

  // The download menu should be shown.
  await eventToPromise('save-menu-shown-for-testing', downloadControls);
  chrome.test.assertTrue(
      mockPlugin.findMessage(getFirstSaveMessageName()) === undefined);
  chrome.test.assertTrue(actionMenu.open);

  const onSave = eventToPromise('save', viewer);

  // Click on "Edited".
  const buttons = actionMenu.querySelectorAll('button');
  chrome.test.assertEq(2, buttons.length);
  buttons[0]!.click();

  // A message should be sent to the plugin to save as annotated.
  await onSave;
  const saveMessage = mockPlugin.findMessage(getFirstSaveMessageName());
  chrome.test.assertTrue(saveMessage !== undefined);
  chrome.test.assertEq(
      getSaveRequestType(saveMessage), SaveRequestType.ANNOTATION);
  chrome.test.assertFalse(actionMenu.open);
}

chrome.test.runTests([
  // Tests that while in annotation mode, on a PDF without any edits, clicking
  // the download button will save the PDF as original.
  async function testSaveOriginal() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    viewerToolbar.setAnnotationMode(AnnotationMode.DRAW);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);

    const downloadControls = getDownloadControls();
    const downloadButton = downloadControls.$.save;
    const actionMenu = downloadControls.$.menu;
    chrome.test.assertFalse(actionMenu.open);

    downloadButton.click();

    // A message should be sent to the plugin to save as original.
    await eventToPromise('save', viewer);
    const saveMessage = mockPlugin.findMessage(getFirstSaveMessageName());
    chrome.test.assertTrue(saveMessage !== undefined);
    chrome.test.assertEq(
        getSaveRequestType(saveMessage), SaveRequestType.ORIGINAL);
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);
    mockMetricsPrivate.assertCount(UserAction.SAVE_ORIGINAL_ONLY, 1);
    chrome.test.succeed();
  },

  // Tests that while in annotation mode, after adding an ink stroke, clicking
  // the download button will prompt the download menu.
  async function testSaveMenuWithStroke() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    const downloadControls = getDownloadControls();
    const actionMenu = downloadControls.$.menu;

    chrome.test.assertFalse(actionMenu.open);

    startFinishModifiedInkStroke(controller);
    await microtasksFinished();
    downloadControls.$.save.click();

    await testSaveWithAnnotations();
    mockMetricsPrivate.assertCount(UserAction.SAVE_WITH_INK2_ANNOTATION, 1);
    chrome.test.succeed();
  },

  // Tests that while in annotation mode, clicking the "Original" save button
  // saves the original PDF.
  async function testSaveOriginalWithStroke() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    const downloadControls = getDownloadControls();
    const actionMenu = downloadControls.$.menu;

    chrome.test.assertFalse(actionMenu.open);

    downloadControls.$.save.click();

    // The download menu should be shown.
    await eventToPromise('save-menu-shown-for-testing', downloadControls);
    chrome.test.assertTrue(
        mockPlugin.findMessage(getFirstSaveMessageName()) === undefined);
    chrome.test.assertTrue(actionMenu.open);

    const onSave = eventToPromise('save', viewer);

    // Click on "Original".
    const buttons = actionMenu.querySelectorAll('button');
    chrome.test.assertEq(2, buttons.length);
    buttons[1]!.click();

    // A message should be sent to the plugin to save as annotated.
    await onSave;
    const saveMessage = mockPlugin.findMessage(getFirstSaveMessageName());
    chrome.test.assertTrue(saveMessage !== undefined);
    chrome.test.assertEq(
        getSaveRequestType(saveMessage), SaveRequestType.ORIGINAL);
    chrome.test.assertFalse(actionMenu.open);
    mockMetricsPrivate.assertCount(UserAction.SAVE_ORIGINAL, 1);
    chrome.test.succeed();
  },

  // Tests that while outside of annotation mode, on a PDF with an ink stroke,
  // clicking the download button will prompt the download menu.
  async function testSaveMenuWithStrokeExitAnnotationMode() {
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);

    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    viewerToolbar.setAnnotationMode(AnnotationMode.OFF);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);

    const downloadControls = getDownloadControls();
    downloadControls.$.menu.close();
    downloadControls.$.save.click();

    await testSaveWithAnnotations();
    mockMetricsPrivate.assertCount(UserAction.SAVE_WITH_INK2_ANNOTATION, 1);
    chrome.test.succeed();
  },

  // An undo operation will remove the only ink stroke from the PDF. Tests that
  // while in annotation mode, after an undo operation, clicking the download
  // button will save the PDF as original.
  async function testSaveOriginalAfterUndo() {
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);

    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    viewerToolbar.setAnnotationMode(AnnotationMode.DRAW);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);

    const undoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#undo');
    chrome.test.assertFalse(undoButton.disabled);

    const downloadControls = getDownloadControls();
    const downloadButton = downloadControls.$.save;
    const actionMenu = downloadControls.$.menu;
    actionMenu.close();

    undoButton.click();
    await microtasksFinished();

    // After undo, there aren't any ink strokes on the PDF, and the button will
    // be disabled.
    chrome.test.assertTrue(undoButton.disabled);

    downloadButton.click();

    // A message should be sent to the plugin to save as original.
    await eventToPromise('save', viewer);
    const saveMessage = mockPlugin.findMessage(getFirstSaveMessageName());
    chrome.test.assertTrue(saveMessage !== undefined);
    chrome.test.assertEq(
        getSaveRequestType(saveMessage), SaveRequestType.ORIGINAL);
    chrome.test.assertFalse(actionMenu.open);
    mockMetricsPrivate.assertCount(UserAction.SAVE_ORIGINAL_ONLY, 1);
    chrome.test.succeed();
  },

  // A redo operation will add the ink stroke back to the PDF. Tests that while
  // in annotation mode, after a redo operation, clicking the download button
  // will prompt the download menu.
  async function testSaveMenuAfterRedo() {
    mockPlugin.clearMessages();

    const redoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#redo');
    chrome.test.assertFalse(redoButton.disabled);

    const downloadControls = getDownloadControls();
    const downloadButton = downloadControls.$.save;
    const actionMenu = downloadControls.$.menu;

    redoButton.click();
    await microtasksFinished();
    downloadButton.click();

    // The download menu should be shown.
    await eventToPromise('save-menu-shown-for-testing', downloadControls);
    chrome.test.assertTrue(
        mockPlugin.findMessage(getFirstSaveMessageName()) === undefined);
    chrome.test.assertTrue(actionMenu.open);
    chrome.test.succeed();
  },

  // Tests that while in annotation mode, after undoing all edits on the PDF,
  // clicking the download button will save the PDF as original.
  async function testSaveOriginalAfterFullUndo() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    // Add another ink stroke. There should now be two ink strokes on the PDF.
    startFinishModifiedInkStroke(controller);
    await microtasksFinished();

    const undoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#undo');
    chrome.test.assertFalse(undoButton.disabled);

    // Undo both ink strokes.
    undoButton.click();
    undoButton.click();
    await microtasksFinished();

    // There shouldn't be any ink strokes on the PDF.
    chrome.test.assertTrue(undoButton.disabled);

    const downloadControls = getDownloadControls();
    const downloadButton = downloadControls.$.save;
    const actionMenu = downloadControls.$.menu;
    actionMenu.close();

    downloadButton.click();

    // A message should be sent to the plugin to save as original.
    await eventToPromise('save', viewer);
    const saveMessage = mockPlugin.findMessage(getFirstSaveMessageName());
    chrome.test.assertTrue(saveMessage !== undefined);
    chrome.test.assertEq(
        getSaveRequestType(saveMessage), SaveRequestType.ORIGINAL);
    chrome.test.assertFalse(actionMenu.open);
    mockMetricsPrivate.assertCount(UserAction.SAVE_ORIGINAL_ONLY, 1);
    chrome.test.succeed();
  },

  // Tests that while in text annotation mode, on a PDF without any edits,
  // clicking the download button will save the PDF as original, even if
  // a textbox is open.
  async function testSaveOriginalInTextMode() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();
    loadTimeData.overrideValues({'pdfTextAnnotationsEnabled': true});
    viewerToolbar.strings = Object.assign({}, viewerToolbar.strings);
    await microtasksFinished();

    viewerToolbar.setAnnotationMode(AnnotationMode.TEXT);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.TEXT, viewerToolbar.annotationMode);
    createTextBox();
    await microtasksFinished();
    const textbox = viewer.shadowRoot.querySelector('ink-text-box');
    chrome.test.assertTrue(!!textbox);
    chrome.test.assertTrue(isVisible(textbox));

    const downloadControls = getDownloadControls();
    const downloadButton = downloadControls.$.save;
    const actionMenu = downloadControls.$.menu;
    chrome.test.assertFalse(actionMenu.open);

    downloadButton.click();

    // A message should be sent to the plugin to save as original.
    await eventToPromise('save', viewer);
    const saveMessage = mockPlugin.findMessage(getFirstSaveMessageName());
    chrome.test.assertTrue(saveMessage !== undefined);
    chrome.test.assertEq(
        getSaveRequestType(saveMessage), SaveRequestType.ORIGINAL);
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(AnnotationMode.TEXT, viewerToolbar.annotationMode);
    mockMetricsPrivate.assertCount(UserAction.SAVE_ORIGINAL_ONLY, 1);

    // Textbox should be hidden.
    chrome.test.assertFalse(isVisible(textbox));
    chrome.test.succeed();
  },

  // Tests that while in text annotation mode, after editing an annotation,
  // clicking the download button will prompt the download menu.
  async function testSaveMenuWithTextBoxOpen() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    const downloadControls = getDownloadControls();
    const actionMenu = downloadControls.$.menu;

    chrome.test.assertFalse(actionMenu.open);

    createTextBox();
    await microtasksFinished();
    const textbox = viewer.shadowRoot.querySelector('ink-text-box');
    chrome.test.assertTrue(!!textbox);
    chrome.test.assertTrue(isVisible(textbox));
    textbox.$.textbox.value = 'Hello';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    downloadControls.$.save.click();

    await testSaveWithAnnotations();
    // Textbox is closed and annotation is committed.
    chrome.test.assertFalse(isVisible(textbox));
    // The finishTextAnnotation message should have been sent before save.
    const saveIndex = mockPlugin.messages.findIndex(
        message => message.type === getFirstSaveMessageName());
    const setTextIndex = mockPlugin.messages.findIndex(
        message => message.type === 'finishTextAnnotation');
    chrome.test.assertFalse(setTextIndex === -1);
    chrome.test.assertTrue(setTextIndex < saveIndex);
    mockMetricsPrivate.assertCount(UserAction.SAVE_WITH_INK2_ANNOTATION, 1);
    chrome.test.succeed();
  },

  // Tests that while in text annotation mode, after undoing all edits on the
  // PDF, clicking the download button will save the PDF as original.
  async function testSaveOriginalAfterFullUndoText() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    // Add another annotation. There should now be 2 text annotations on the
    // PDF.
    PluginController.getInstance().getEventTarget().dispatchEvent(
        new CustomEvent(PluginControllerEventType.FINISH_INK_STROKE));
    await microtasksFinished();

    const undoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#undo');
    chrome.test.assertFalse(undoButton.disabled);

    // Undo both text annotations.
    undoButton.click();
    undoButton.click();
    await microtasksFinished();

    // There shouldn't be any ink strokes or text annotations on the PDF.
    chrome.test.assertTrue(undoButton.disabled);

    const downloadControls = getDownloadControls();
    const downloadButton = downloadControls.$.save;
    const actionMenu = downloadControls.$.menu;
    actionMenu.close();

    downloadButton.click();

    // A message should be sent to the plugin to save as original.
    await eventToPromise('save', viewer);
    const saveMessage = mockPlugin.findMessage(getFirstSaveMessageName());
    chrome.test.assertTrue(saveMessage !== undefined);
    chrome.test.assertEq(
        getSaveRequestType(saveMessage), SaveRequestType.ORIGINAL);
    chrome.test.assertFalse(actionMenu.open);
    mockMetricsPrivate.assertCount(UserAction.SAVE_ORIGINAL_ONLY, 1);
    chrome.test.succeed();
  },

  // A redo operation will add the text annotation back to the PDF. Tests that
  // while in annotation mode, after a redo operation, clicking the download
  // button will prompt the download menu.
  async function testSaveMenuAfterRedoText() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();
    // Set a reply for the save message that will bypass opening a file dialog
    // and saving the data to disk.
    mockPlugin.setReplyToSave(true);

    const redoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#redo');
    chrome.test.assertFalse(redoButton.disabled);

    const downloadControls = getDownloadControls();
    const downloadButton = downloadControls.$.save;

    redoButton.click();
    await microtasksFinished();
    mockMetricsPrivate.assertCount(UserAction.SAVE_WITH_INK2_ANNOTATION, 0);

    downloadButton.click();
    await testSaveWithAnnotations();
    mockMetricsPrivate.assertCount(UserAction.SAVE_WITH_INK2_ANNOTATION, 1);

    // The test should be able to successfully exit, as PDF Viewer should have
    // turned off the beforeunload dialog after the successful save.
    chrome.test.succeed();
  },
]);
