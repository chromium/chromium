// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens-overlay/find_words_in_region.js';

import {areaOfPolygon, clip, ClippingEdge, findWordsInRegion, intersectionWithEdge, isInsideEdge, rotate, toPolygon} from 'chrome-untrusted://lens-overlay/find_words_in_region.js';
import {CenterRotatedBox_CoordinateType} from 'chrome-untrusted://lens-overlay/geometry.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createWord} from '../utils/text_utils.js';

suite('FindWordsInRegion', function() {
  test('findWordsInRegion', () => {
    const words = [
      createWord('one', {x: 0.25, y: 0.25, width: 0.5, height: 0.25}),
      createWord('two', {x: 0.75, y: 0.5, width: 0.5, height: 0.25}),
      createWord('three', {x: 0.25, y: 0.25, width: 0.5, height: 0.25}),
      createWord('four', {x: 0.75, y: 0.5, width: 0.5, height: 0.25}),
    ];
    const selectionRegion = {
      box: {x: 0.75, y: 0.375, width: 0.375, height: 0.5},
      coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
      rotation: 0,
    };
    const imageBounds = new DOMRect(12, 34, 100, 200);

    const result = findWordsInRegion(words, selectionRegion, imageBounds);

    const expectedResult = {startIndex: 1, endIndex: 3, iou: 0.75};
    assertDeepEquals(expectedResult, result);
  });

  test('toPolygon', () => {
    const box = {
      box: {x: 0.75, y: 0.5, width: 0.75, height: 0.25},
      coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
      rotation: Math.PI / 2,
    };
    const imageBounds = new DOMRect(12, 34, 100, 200);

    const polygon = toPolygon(box, imageBounds);

    const expectedPolygon = [
      {x: 1, y: 0.3125},
      {x: 1, y: 0.6875},
      {x: 0.5, y: 0.6875},
      {x: 0.5, y: 0.3125},
    ];
    assertDeepEquals(expectedPolygon, polygon);
  });

  test('areaOfPolygon', () => {
    const parallelogram = [
      {x: 0.25, y: 0.75},
      {x: 0.5, y: 0.5},
      {x: 0.75, y: 0.5},
      {x: 0.5, y: 0.75},
    ];

    assertEquals(0.0625, areaOfPolygon(parallelogram));
  });

  test('rotate accounts for non-square image bounds', () => {
    const anchor = {x: 0.5, y: 0.5};
    const angle_radian = Math.PI / 2;
    const target = {x: 1, y: 0.5};
    const imageBounds = new DOMRect(12, 34, 100, 200);

    const result = rotate(anchor, angle_radian, target, imageBounds);

    assertDeepEquals({x: 0.5, y: 0.75}, result);
  });

  test('clip', () => {
    const diamond = [
      {x: 0.125, y: 0.625},
      {x: 0.5, y: 0.25},
      {x: 0.875, y: 0.625},
      {x: 0.5, y: 1},
    ];
    const selectionBounds =
        {left: 0.25, right: 0.75, top: 0.375, bottom: 0.875};

    const octagon = clip(diamond, selectionBounds);

    const expectedOctagon = [
      {x: 0.375, y: 0.875},
      {x: 0.25, y: 0.75},
      {x: 0.25, y: 0.5},
      {x: 0.375, y: 0.375},
      {x: 0.625, y: 0.375},
      {x: 0.75, y: 0.5},
      {x: 0.75, y: 0.75},
      {x: 0.625, y: 0.875},
    ];
    assertDeepEquals(expectedOctagon, octagon);
  });

  test('isInsideEdge', () => {
    const selectionBounds = {left: 0.2, right: 0.6, top: 0.4, bottom: 0.8};

    assertFalse(
        isInsideEdge({x: 0.1, y: 0.6}, selectionBounds, ClippingEdge.LEFT));
    assertTrue(
        isInsideEdge({x: 0.9, y: 0.6}, selectionBounds, ClippingEdge.LEFT));
    assertTrue(
        isInsideEdge({x: 0.1, y: 0.6}, selectionBounds, ClippingEdge.RIGHT));
    assertFalse(
        isInsideEdge({x: 0.9, y: 0.6}, selectionBounds, ClippingEdge.RIGHT));
    assertFalse(
        isInsideEdge({x: 0.4, y: 0.1}, selectionBounds, ClippingEdge.TOP));
    assertTrue(
        isInsideEdge({x: 0.4, y: 0.9}, selectionBounds, ClippingEdge.TOP));
    assertTrue(
        isInsideEdge({x: 0.4, y: 0.1}, selectionBounds, ClippingEdge.BOTTOM));
    assertFalse(
        isInsideEdge({x: 0.4, y: 0.9}, selectionBounds, ClippingEdge.BOTTOM));
  });

  test('intersectionWithEdge', () => {
    const v0 = {x: 0, y: 1};
    const v1 = {x: 1, y: 0};
    const selectionBounds = {left: 0.2, right: 0.4, top: 0.1, bottom: 0.3};

    const intersectionLeft =
        intersectionWithEdge(v0, v1, selectionBounds, ClippingEdge.LEFT);
    const intersectionRight =
        intersectionWithEdge(v0, v1, selectionBounds, ClippingEdge.RIGHT);
    const intersectionTop =
        intersectionWithEdge(v0, v1, selectionBounds, ClippingEdge.TOP);
    const intersectionBottom =
        intersectionWithEdge(v0, v1, selectionBounds, ClippingEdge.BOTTOM);

    assertDeepEquals({x: 0.2, y: 0.8}, intersectionLeft);
    assertDeepEquals({x: 0.4, y: 0.6}, intersectionRight);
    assertDeepEquals({x: 0.9, y: 0.1}, intersectionTop);
    assertDeepEquals({x: 0.7, y: 0.3}, intersectionBottom);
  });
});
