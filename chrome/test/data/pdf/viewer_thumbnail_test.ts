// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ViewerThumbnailElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

function createThumbnail() {
  document.body.innerHTML = '';
  const thumbnail = document.createElement('viewer-thumbnail');
  document.body.appendChild(thumbnail);
  return thumbnail;
}

function createPdfCanvas(
    thumbnail: ViewerThumbnailElement, imageSize: number[]) {
  thumbnail.image = new ImageData(imageSize[0]!, imageSize[1]!);
  const divId = '#pdf-canvas';
  return thumbnail.shadowRoot!.querySelector<HTMLCanvasElement>(divId)!;
}

// <if expr="enable_pdf_ink2">
function createInk2Canvas(
    thumbnail: ViewerThumbnailElement, imageSize: number[]) {
  thumbnail.ink2Image = new ImageData(imageSize[0]!, imageSize[1]!);
  const divId = '#ink2-canvas';
  return thumbnail.shadowRoot!.querySelector<HTMLCanvasElement>(divId)!;
}
// </if>

function checkThumbnailAncestorDivSize(
    canvas: HTMLCanvasElement, divSize: number[]) {
  // The parent <div> is only there to handle opacity, so ignore it.
  // The ancestor <div> to test is the grandparent.
  const div = canvas.parentElement!.parentElement!;
  chrome.test.assertEq(divSize[0], div.offsetWidth);
  chrome.test.assertEq(divSize[1], div.offsetHeight);
}

function checkThumbnailSize(canvas: HTMLCanvasElement, canvasSize: number[]) {
  chrome.test.assertEq(`${canvasSize[0]}px`, canvas.style.width);
  chrome.test.assertEq(`${canvasSize[1]}px`, canvas.style.height);
}

function testThumbnailSize(
    thumbnail: ViewerThumbnailElement, imageSize: number[],
    canvasSize: number[]) {
  const canvas = createPdfCanvas(thumbnail, imageSize);
  checkThumbnailSize(canvas, canvasSize);

  // The thumbnail div ancestor containing the canvas should be resized to fit.
  checkThumbnailAncestorDivSize(canvas, canvasSize);
}

// <if expr="enable_pdf_ink2">
function testInk2ThumbnailSize(
    thumbnail: ViewerThumbnailElement, imageSize: number[],
    canvasSize: number[]) {
  const canvas = createInk2Canvas(thumbnail, imageSize);
  checkThumbnailSize(canvas, canvasSize);

  // The thumbnail div ancestor containing the canvas should be resized to fit.
  checkThumbnailAncestorDivSize(canvas, canvasSize);
}
// </if>

function checkRotatedThumbnailSizeAndTransform(
    canvas: HTMLCanvasElement, clockwiseRotations: number, divSize: number[]) {
  const halfTurn = clockwiseRotations % 2 === 0;
  chrome.test.assertEq(
      `${halfTurn ? divSize[0] : divSize[1]}px`, canvas.style.width);
  chrome.test.assertEq(
      `${halfTurn ? divSize[1] : divSize[0]}px`, canvas.style.height);

  // The thumbnail div ancestor containing the canvas should be resized to fit.
  checkThumbnailAncestorDivSize(canvas, divSize);

  chrome.test.assertEq(
      `rotate(${clockwiseRotations * 90}deg)`, canvas.style.transform);
}

async function testThumbnailRotations(
    imageSize: number[], rotatedDivSizes: number[][]) {
  const thumbnail = createThumbnail();
  const canvas = createPdfCanvas(thumbnail, imageSize);
  // <if expr="enable_pdf_ink2">
  const inkCanvas = createInk2Canvas(thumbnail, imageSize);
  // </if>

  chrome.test.assertEq(4, rotatedDivSizes.length);
  for (let rotations = 0; rotations < rotatedDivSizes.length; rotations++) {
    thumbnail.clockwiseRotations = rotations;
    await microtasksFinished();
    checkRotatedThumbnailSizeAndTransform(
        canvas, rotations, rotatedDivSizes[rotations]!);
    // <if expr="enable_pdf_ink2">
    checkRotatedThumbnailSizeAndTransform(
        inkCanvas, rotations, rotatedDivSizes[rotations]!);
    // </if>
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
              }) => {
      testThumbnailSize(thumbnail, imageSize, canvasSize);
      // <if expr="enable_pdf_ink2">
      testInk2ThumbnailSize(thumbnail, imageSize, canvasSize);
      // </if>
    });

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
  async function testRotateNormalLowRes() {
    window.devicePixelRatio = 1;

    // Letter
    await testThumbnailRotations(
        [108, 140], [[108, 140], [140, 108], [108, 140], [140, 108]]);

    // A4
    await testThumbnailRotations(
        [108, 152], [[108, 152], [140, 99], [108, 152], [140, 99]]);

    chrome.test.succeed();
  },
  async function testRotateNormalHighRes() {
    window.devicePixelRatio = 2;

    // Letter
    await testThumbnailRotations(
        [216, 280], [[108, 140], [140, 108], [108, 140], [140, 108]]);

    // A4
    await testThumbnailRotations(
        [216, 304], [[108, 152], [140, 99], [108, 152], [140, 99]]);

    chrome.test.succeed();
  },
  async function testRotateNormalHighRes() {
    window.devicePixelRatio = 1;

    await testThumbnailRotations(
        [50, 1500], [[50, 1500], [140, 4], [50, 1500], [140, 4]]);

    chrome.test.succeed();
  },
  async function testRotateNormalHighRes() {
    window.devicePixelRatio = 2;

    await testThumbnailRotations(
        [50, 1500], [[25, 750], [140, 4], [25, 750], [140, 4]]);

    chrome.test.succeed();
  },
  async function testContextMenuDisabled() {
    // Set some image data so a canvas is created inside the thumbnail.
    const thumbnail = createThumbnail();
    {
      const canvas = createPdfCanvas(thumbnail, [108, 140]);

      const whenContextMenu = eventToPromise('contextmenu', canvas);
      canvas.dispatchEvent(new CustomEvent('contextmenu', {cancelable: true}));
      const e = await whenContextMenu;

      chrome.test.assertTrue(e.defaultPrevented);
    }
    // <if expr="enable_pdf_ink2">
    {
      const canvas = createInk2Canvas(thumbnail, [108, 140]);

      const whenContextMenu = eventToPromise('contextmenu', canvas);
      canvas.dispatchEvent(new CustomEvent('contextmenu', {cancelable: true}));
      const e = await whenContextMenu;

      chrome.test.assertTrue(e.defaultPrevented);
    }
    // </if>
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
