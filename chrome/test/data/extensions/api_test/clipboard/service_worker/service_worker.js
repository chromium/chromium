// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Draws a red square on a white background and returns it as a PNG blob.
async function makePngBlob() {
  const size = 200;
  const canvas = new OffscreenCanvas(size, size);
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = 'white';
  ctx.fillRect(0, 0, size, size);
  ctx.fillStyle = 'red';
  ctx.fillRect(50, 50, 100, 100);
  return canvas.convertToBlob();
}

function writeBlob(type, contents) {
  const blob = new Blob([contents], {type});
  return navigator.clipboard.write([new ClipboardItem({[type]: blob})]);
}

chrome.test.runTests([
  async function testClipboardIsExposed() {
    chrome.test.assertTrue(
        !!navigator.clipboard, 'navigator.clipboard should be defined');
    chrome.test.succeed();
  },

  async function testReadRejectedInWorker() {
    await chrome.test.assertPromiseRejects(
        navigator.clipboard.readText(),
        /NotAllowedError.*Read not supported in Worker/);
    chrome.test.succeed();
  },

  // HTML and SVG sanitization goes through DOMParser, which needs a frame, so
  // both formats are rejected when written from a worker.
  async function testHtmlWriteRejectedInWorker() {
    await chrome.test.assertPromiseRejects(
        writeBlob('text/html', '<h1>Hello</h1>'),
        /NotAllowedError.*text\/html not supported on write/);
    chrome.test.succeed();
  },

  async function testSvgWriteRejectedInWorker() {
    await chrome.test.assertPromiseRejects(
        writeBlob('image/svg+xml', '<svg></svg>'),
        /NotAllowedError.*image\/svg\+xml not supported on write/);
    chrome.test.succeed();
  },

  async function testWriteImage() {
    const blob = await makePngBlob();
    await navigator.clipboard.write([new ClipboardItem({[blob.type]: blob})]);
    chrome.test.succeed();
  },

  // Runs last so the browser test can assert on the final clipboard contents.
  async function testWriteText() {
    await navigator.clipboard.writeText('Hello from Service Worker!');
    chrome.test.succeed();
  },
]);
