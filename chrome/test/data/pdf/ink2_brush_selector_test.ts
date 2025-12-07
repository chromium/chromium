// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationBrushType, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {InkBrushSelectorElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {setupMockMetricsPrivate} from './test_util.js';

const mockMetricsPrivate = setupMockMetricsPrivate();

function createSelector(): InkBrushSelectorElement {
  const selector = document.createElement('ink-brush-selector');
  document.body.innerHTML = '';
  document.body.appendChild(selector);
  return selector;
}

/**
 * Tests that the correct brush icons are displayed, depending on what brush
 * is selected. The brush type matching `selectedBrushType` should have a filled
 * icon.
 * @param selector The ink brush selector element.
 * @param selectedBrushType The expected selected brush type that should
 * have a filled icon.
 */
function assertBrushIcons(
    selector: InkBrushSelectorElement, selectedBrushType: AnnotationBrushType) {
  const eraserIcon = selector.$.eraser.icon;
  chrome.test.assertTrue(!!eraserIcon);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.ERASER ?
          'pdf-ink:ink-eraser-fill' :
          'pdf-ink:ink-eraser',
      eraserIcon);

  const highlighterIcon = selector.$.highlighter.icon;
  chrome.test.assertTrue(!!highlighterIcon);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.HIGHLIGHTER ?
          'pdf-ink:ink-highlighter-fill' :
          'pdf-ink:ink-highlighter',
      highlighterIcon);

  const penIcon = selector.$.pen.icon;
  chrome.test.assertTrue(!!penIcon);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.PEN ? 'pdf-ink:ink-pen-fill' :
                                                      'pdf-ink:ink-pen',
      penIcon);
}

/**
 * Tests that the brushes have correct values for the selected attribute. The
 * brush type matching `selectedBrushType` should be selected.
 * @param selector The ink brush selector element.
 * @param selectedBrushType The expected selected brush type.
 */
function assertSelectedBrush(
    selector: InkBrushSelectorElement, selectedBrushType: AnnotationBrushType) {
  const eraserSelected = selector.$.eraser.checked;
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.ERASER, eraserSelected);

  const highlighterSelected = selector.$.highlighter.checked;
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.HIGHLIGHTER,
      highlighterSelected);

  const penSelected = selector.$.pen.checked;
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.PEN, penSelected);
}

chrome.test.runTests([
  async function testSelectPen() {
    mockMetricsPrivate.reset();

    const selector = createSelector();
    selector.$.pen.click();
    await microtasksFinished();

    assertBrushIcons(selector, AnnotationBrushType.PEN);
    chrome.test.assertEq(selector.$.pen.label, 'Pen');
    assertSelectedBrush(selector, AnnotationBrushType.PEN);
    mockMetricsPrivate.assertCount(UserAction.SELECT_INK2_BRUSH_PEN, 0);
    chrome.test.succeed();
  },
  async function testSelectHighlighter() {
    mockMetricsPrivate.reset();

    const selector = createSelector();
    selector.$.highlighter.click();
    await microtasksFinished();

    assertBrushIcons(selector, AnnotationBrushType.HIGHLIGHTER);
    chrome.test.assertEq(selector.$.highlighter.label, 'Highlighter');
    assertSelectedBrush(selector, AnnotationBrushType.HIGHLIGHTER);
    mockMetricsPrivate.assertCount(UserAction.SELECT_INK2_BRUSH_HIGHLIGHTER, 1);
    chrome.test.succeed();
  },
  async function testSelectEraser() {
    mockMetricsPrivate.reset();

    const selector = createSelector();
    selector.$.eraser.click();
    await microtasksFinished();

    assertBrushIcons(selector, AnnotationBrushType.ERASER);
    chrome.test.assertEq(selector.$.eraser.label, 'Eraser');
    assertSelectedBrush(selector, AnnotationBrushType.ERASER);
    mockMetricsPrivate.assertCount(UserAction.SELECT_INK2_BRUSH_ERASER, 1);
    chrome.test.succeed();
  },
  async function testSelectBackToPen() {
    mockMetricsPrivate.reset();

    const selector = createSelector();
    selector.$.eraser.click();
    await microtasksFinished();

    selector.$.pen.click();
    await microtasksFinished();

    assertBrushIcons(selector, AnnotationBrushType.PEN);
    chrome.test.assertEq(selector.$.pen.label, 'Pen');
    assertSelectedBrush(selector, AnnotationBrushType.PEN);
    mockMetricsPrivate.assertCount(UserAction.SELECT_INK2_BRUSH_ERASER, 1);
    mockMetricsPrivate.assertCount(UserAction.SELECT_INK2_BRUSH_PEN, 1);
    chrome.test.succeed();
  },
]);
