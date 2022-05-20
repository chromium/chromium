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

window.onload =
    function() {
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
    },

    // Verify that the requested icon size is returned.
    async function cachedResolutions() {
      const port = (await chrome.test.getConfig()).testServer.port;
      const pixelsArr = [16, 32, 64, 48];
      const promises = pixelsArr.map(pixels => {
        return new Promise((resolve, reject) => {
          const favicon = new Favicon({
            pageUrl: `http://www.example.com:${
                port}/extensions/favicon/test_file.html`,
            size: pixels
          });
          const image = new Image();
          image.src = favicon.getUrl();
          image.onload = () => {
            chrome.test.assertEq(pixels, image.height);
            chrome.test.assertEq(pixels, image.width);
            resolve();
          }
        });
      });
      Promise.all(promises).then(() => chrome.test.succeed());
    },

    // The default favicon size is 16.
    async function defaultSize() {
      const port = (await chrome.test.getConfig()).testServer.port;
      const expected = 16;
      const favicon = new Favicon({
        pageUrl:
            `http://www.example.com:${port}/extensions/favicon/test_file.html`
      });
      const image = new Image();
      image.src = favicon.getUrl();
      image.onload = () => {
        chrome.test.assertEq(expected, image.height);
        chrome.test.assertEq(expected, image.width);
        chrome.test.succeed();
      }
    },

    // Uncached icon test. Default icons are 16 pixels. 32 or 64 are supported.
    async function defaultResolution() {
      const pixels = 48;
      const port = (await chrome.test.getConfig()).testServer.port;
      const favicon = new Favicon({
        pageUrl: `http://www.unvisited.com:${
            port}/extensions/favicon/test_file.html`,
        size: pixels
      });
      const image = new Image();
      image.src = favicon.getUrl();
      image.onload = function() {
        const expected = 16;
        chrome.test.assertEq(expected, this.height);
        chrome.test.assertEq(expected, this.width);
        chrome.test.succeed();
      }
    },

    // We can't use the page's favicon file
    // (chrome/test/data/extensions/favicon/favicon.ico) directly because .ico
    // files appear to consistently show up as a blank canvas, possibly due to
    // some of the graphics stack being stubbed out in browser tests. The bmp
    // here is a direct conversion of the .ico file.
    async function bmpMatch() {
      const port = (await chrome.test.getConfig()).testServer.port;
      const urls = [
        '_test_resources/favicon/favicon.bmp',
        new Favicon({
          pageUrl: `http://www.example.com:${
              port}/extensions/favicon/test_file.html`,
          size: 48
        }).getUrl()
      ];
      const promises = [];
      urls.forEach(url => promises.push(toDataURL(url)));
      Promise.all(promises).then(values => {
        // If this test becomes fragile (e.g. due to favicon service changes),
        // it could be restructured to just compare a single pixel of a
        // one-color favicon instead of looking for pixel-to-pixel accuracy.
        chrome.test.assertEq(values[0], values[1]);
        chrome.test.succeed();
      });
    },
  ]);
}

// Fetches the data from url and encodes the retrieved data into a data URL.
function toDataURL(url) {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    xhr.open('get', url);
    xhr.responseType = 'blob';
    xhr.onload = () => {
      const fr = new FileReader();
      fr.onload = () => resolve(this.result);
      fr.readAsDataURL(xhr.response);
    };
    xhr.send();
  });
}
