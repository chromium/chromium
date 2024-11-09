// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationBrushType, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {InkBrushSelectorElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
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
  const eraserIcon = selector.$.eraser.getAttribute('iron-icon');
  assert(eraserIcon);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.ERASER ? 'pdf:ink-eraser-fill' :
                                                         'pdf:ink-eraser',
      eraserIcon);

  const highlighterIcon = selector.$.highlighter.getAttribute('iron-icon');
  assert(highlighterIcon);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.HIGHLIGHTER ?
          'pdf:ink-highlighter-fill' :
          'pdf:ink-highlighter',
      highlighterIcon);

  const penIcon = selector.$.pen.getAttribute('iron-icon');
  assert(penIcon);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.PEN ? 'pdf:ink-pen-fill' :
                                                      'pdf:ink-pen',
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
  const eraserSelected = selector.$.eraser.dataset['selected'];
  assert(eraserSelected);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.ERASER ? 'true' : 'false',
      eraserSelected);

  const highlighterSelected = selector.$.highlighter.dataset['selected'];
  assert(highlighterSelected);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.HIGHLIGHTER ? 'true' : 'false',
      highlighterSelected);

  const penSelected = selector.$.pen.dataset['selected'];
  assert(penSelected);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.PEN ? 'true' : 'false',
      penSelected);
}

chrome.test.runTests([
  async function testSelectPen() {
    mockMetricsPrivate.reset();

    const selector = createSelector();
    selector.$.pen.click();
    await microtasksFinished();

    assertBrushIcons(selector, AnnotationBrushType.PEN);
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
    assertSelectedBrush(selector, AnnotationBrushType.PEN);
    mockMetricsPrivate.assertCount(UserAction.SELECT_INK2_BRUSH_ERASER, 1);
    mockMetricsPrivate.assertCount(UserAction.SELECT_INK2_BRUSH_PEN, 1);
    chrome.test.succeed();
  },
]);
