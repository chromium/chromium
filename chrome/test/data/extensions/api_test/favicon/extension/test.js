// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Favicon {
  constructor(obj) {
    // Page of the favicon. page_ or icon_ must be provided. page_ wins.
    this.pageUrl;

    // Optional. Favicon size in pixels. Backend default: 16.
    this.size;

    // Update this favicon object from constructor parameter object.
    obj && Object.keys(obj).forEach(key => this[key] = obj[key]);
  }

  // Returns a constructed URL with properties set as query parameters.
  getUrl(baseUrl) {
    let result = [];
    Object.keys(this).forEach(key => {
      // The backend expects snake_case query parameters.
      const snakeCase = key.replace(/[A-Z]/g, c => `_${c.toLowerCase()}`);
      result.push(`${snakeCase}=${this[key]}`);
    });
    const url =
        baseUrl ? baseUrl : `chrome-extension://${chrome.runtime.id}/_favicon/`;
    return `${url}?${result.join('&')}`;
  }
};

window.onload = function() {
  chrome.test.runTests([
    // Asynchronously fetch favicon.
    async function all() {
      const config = await chrome.test.getConfig();
      const port = config.testServer.port;
      const testCases = [
        [
          'Load favicon using only pageUrl', true, new Favicon({
            pageUrl: `http://www.example.com:${
                port}/extensions/favicon/test_file.html`
          })
        ],
        [
          'Load favicon using multiple arguments', true, new Favicon({
            pageUrl: `http://www.example.com:${
                port}/extensions/favicon/test_file.html`,
            size: 16,
            scaleFactor: '1x'
          })
        ],
        [
          'Get the default icon when a url hasn\'t been visited', true,
          new Favicon({
            pageUrl: `http://www.unvisited.com:${
                port}/extensions/favicon/test_file.html`
          })
        ],
        [
          'Incorrect baseUrl', false, new Favicon({
            pageUrl: `http://www.example.com:${
                port}/extensions/favicon/test_file.html`
          }),
          `chrome-extension://${chrome.runtime.id}/_faviconbutnotreally/`
        ],
        [
          'Slash not required before question mark query params', true,
          new Favicon({
            pageUrl: `http://www.example.com:${
                port}/extensions/favicon/test_file.html`
          }),
          `chrome-extension://${chrome.runtime.id}/_favicon`
        ],
      ];
      let promises = [];
      testCases.forEach(testCase => {
        promises.push(new Promise(resolve => {
          const [title, isOk, favicon, baseUrl = null] = testCase;
          const img = document.createElement('img');
          document.body.appendChild(img);
          img.onload = () => isOk ? resolve() : chrome.test.fail(title);
          img.onerror = () => isOk ? chrome.test.fail(title) : resolve();
          img.src = favicon.getUrl(baseUrl);
        }));
      });
      Promise.all(promises).then(() => chrome.test.succeed());
    }
  ]);
}
