/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Opens the given `url` in an iframe, posts the `msg` to it, and waits for the
 * iframe to return a response.
 * @param {string} url - The url to open in the iframe.
 * @param {object} msg - The message to post to the iframe.
 * @return {Promise<object>} - What the iframe returned.
 */
async function postToIframe(url, msg) {
  let resolveFunction = null;
  const promise = new Promise((resolve) => {
    resolveFunction = resolve;
  });

  window.onmessage = (e) => {
    resolveFunction(e.data);
  };

  const iframe = document.getElementById('iframe');
  iframe.onload = (e) => {
    iframe.contentWindow.postMessage(msg, url);
  };
  iframe.src = url;

  return promise;
}
