// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ViewerThumbnailElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

function createThumbnail() {
  document.body.innerHTML = '';
  const thumbnail = document.createElement('viewer-thumbnail');
  document.body.appendChild(thumbnail);
  return thumbnail;
}

function testThumbnailSize(
    thumbnail: ViewerThumbnailElement, imageSize: number[],
    canvasSize: number[]) {
  const imageData = new ImageData(imageSize[0]!, imageSize[1]!);
  thumbnail.image = imageData;

  const canvas = thumbnail.shadowRoot!.querySelector('canvas')!;
  chrome.test.assertEq(`${canvasSize[0]}px`, canvas.style.width);
  chrome.test.assertEq(`${canvasSize[1]}px`, canvas.style.height);

  // The div containing the canvas should be resized to fit.
  const div = canvas.parentElement!;
  chrome.test.assertEq(canvasSize[0], div.offsetWidth);
  chrome.test.assertEq(canvasSize[1], div.offsetHeight);
}

function testThumbnailRotation(
    thumbnail: ViewerThumbnailElement, clockwiseRotations: number,
    divSize: number[]) {
  thumbnail.clockwiseRotations = clockwiseRotations;

  const canvas = thumbnail.shadowRoot!.querySelector('canvas')!;
  const halfTurn = clockwiseRotations % 2 === 0;
  chrome.test.assertEq(
      `${halfTurn ? divSize[0] : divSize[1]}px`, canvas.style.width);
  chrome.test.assertEq(
      `${halfTurn ? divSize[1] : divSize[0]}px`, canvas.style.height);

  // The div containing the rotated canvas should be resized to fit.
  const div = canvas.parentElement!;
  chrome.test.assertEq(divSize[0], div.offsetWidth);
  chrome.test.assertEq(divSize[1], div.offsetHeight);

  chrome.test.assertEq(
      `rotate(${clockwiseRotations * 90}deg)`, canvas.style.transform);
}

function testThumbnailRotations(
    imageSize: number[], rotatedDivSizes: number[][]) {
  const thumbnail = createThumbnail();
  const imageData = new ImageData(imageSize[0]!, imageSize[1]!);
  thumbnail.image = imageData;

  chrome.test.assertEq(4, rotatedDivSizes.length);
  for (let rotations = 0; rotations < rotatedDivSizes.length; rotations++) {
    testThumbnailRotation(thumbnail, rotations, rotatedDivSizes[rotations]!);
  }
}

const tests = [
  function testSetNormalImageLowRes() {
    window.devicePixelRatio = 1;
    const thumbnail = createThumbnail();

    [
        // Letter portrait
        {imageSize: [108, 140], canvasSize: [108, 140]},
        // Letter landscape
        {imageSize: [140, 108], canvasSize: [140, 108]},
        // A4 portrait
        {imageSize: [108, 152], canvasSize: [108, 152]},
        // A4 portrait
        {imageSize: [152, 108], canvasSize: [140, 99]},
    ].forEach(({
                imageSize,
                canvasSize,
              }) => testThumbnailSize(thumbnail, imageSize, canvasSize));

    chrome.test.succeed();
  },
  function testSetNormalImageHighRes() {
    window.devicePixelRatio = 2;
    const thumbnail = createThumbnail();

    [
        // Letter portrait
        {imageSize: [216, 280], canvasSize: [108, 140]},
        // Letter landscape
        {imageSize: [280, 216], canvasSize: [140, 108]},
        // A4 portrait
        {imageSize: [216, 304], canvasSize: [108, 152]},
        // A4 portrait
        {imageSize: [304, 216], canvasSize: [140, 99]},
    ].forEach(({
                imageSize,
                canvasSize,
              }) => testThumbnailSize(thumbnail, imageSize, canvasSize));

    chrome.test.succeed();
  },
  function testSetExtremeImageLowRes() {
    window.devicePixelRatio = 1;
    const thumbnail = createThumbnail();

    [
        // The image should not scale to preserve its resolution.
        {imageSize: [50, 1500], canvasSize: [50, 1500]},
        // The image should scale down to fit in the sidenav.
        {imageSize: [1500, 50], canvasSize: [140, 4]},
    ].forEach(({
                imageSize,
                canvasSize,
              }) => testThumbnailSize(thumbnail, imageSize, canvasSize));

    chrome.test.succeed();
  },
  function testSetExtremeImageHighRes() {
    window.devicePixelRatio = 2;
    const thumbnail = createThumbnail();

    [
        // The image should scale down to preserve its resolution.
        {imageSize: [50, 1500], canvasSize: [25, 750]},
        // The image should scale down to fit in the sidenav.
        {imageSize: [1500, 50], canvasSize: [140, 4]},
    ].forEach(({
                imageSize,
                canvasSize,
              }) => testThumbnailSize(thumbnail, imageSize, canvasSize));

    chrome.test.succeed();
  },
  function testRotateNormalLowRes() {
    window.devicePixelRatio = 1;

    // Letter
    testThumbnailRotations(
        [108, 140], [[108, 140], [140, 108], [108, 140], [140, 108]]);

    // A4
    testThumbnailRotations(
        [108, 152], [[108, 152], [140, 99], [108, 152], [140, 99]]);

    chrome.test.succeed();
  },
  function testRotateNormalHighRes() {
    window.devicePixelRatio = 2;

    // Letter
    testThumbnailRotations(
        [216, 280], [[108, 140], [140, 108], [108, 140], [140, 108]]);

    // A4
    testThumbnailRotations(
        [216, 304], [[108, 152], [140, 99], [108, 152], [140, 99]]);

    chrome.test.succeed();
  },
  function testRotateNormalHighRes() {
    window.devicePixelRatio = 1;

    testThumbnailRotations(
        [50, 1500], [[50, 1500], [140, 4], [50, 1500], [140, 4]]);

    chrome.test.succeed();
  },
  function testRotateNormalHighRes() {
    window.devicePixelRatio = 2;

    testThumbnailRotations(
        [50, 1500], [[25, 750], [140, 4], [25, 750], [140, 4]]);

    chrome.test.succeed();
  },
  async function testContextMenuDisabled() {
    // Set some image data so a canvas is created inside the thumbnail.
    const thumbnail = createThumbnail();
    thumbnail.image = new ImageData(108, 140);
    const canvas = thumbnail.shadowRoot!.querySelector('canvas')!;

    const whenContextMenu = eventToPromise('contextmenu', canvas);
    canvas.dispatchEvent(new CustomEvent('contextmenu', {cancelable: true}));
    const e = await whenContextMenu;

    chrome.test.assertTrue(e.defaultPrevented);
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
