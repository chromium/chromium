/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Loads the given `url` into an iframe and then removes it after the `timeout`.
 * @param {string} url - The URL to load into the iframe.
 * @param {int} timeout - The number of milliseconds to wait before removing the
 * iframe.
 * @return {Promise<string>} - The string "success".
 */
async function loadAndRemoveIframe(url, timeout) {
  const frame = document.getElementById('ifrm');
  frame.src = url;
  return new Promise((resolve) => {
    frame.onload = () => {
      window.setTimeout(() => {
        frame.parentNode.removeChild(frame);
        resolve('success');
      }, timeout);
    };
  });
}
