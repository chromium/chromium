// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var port;
var visitedPageUrl;

window.onload =
    function() {
  chrome.test.runTests([
    async function init() {
      port = (await chrome.test.getConfig()).testServer.port;
      visitedPageUrl =
          `http://www.example.com:${port}/extensions/favicon/test_file.html`;
      chrome.test.succeed();
    },

    // Asynchronously fetch favicon in various ways.
    function various() {
      const testCases = [
        [
          'Load favicon using only pageUrl', true,
          `_favicon/?pageUrl=${visitedPageUrl}`
        ],
        [
          'Succeed with chrome.runtime.getURL', true,
          chrome.runtime.getURL(`_favicon/?pageUrl=${visitedPageUrl}`)
        ],
        [
          'Load favicon using multiple arguments', true,
          `_favicon/?pageUrl=${visitedPageUrl}&size=16&scaleFactor=1x`
        ],
        [
          'Get the default icon when a url hasn\'t been visited', true,
          `_favicon/?pageUrl=http://www.unvisited.com:${
              port}/extensions/favicon/test_file.html`
        ],
        [
          'Incorrect baseUrl', false,
          `chrome-extension://${
              chrome.runtime.id}/_faviconbutnotreally/?pageUrl=${
              visitedPageUrl}`
        ],
        [
          'Slash not required before question mark query params', true,
          `chrome-extension://${chrome.runtime.id}/_favicon/?pageUrl=${
              visitedPageUrl}`
        ],
        [
          'pageUrl must be present and iconUrl is ignored', false,
          `_favicon/?iconUrl=${visitedPageUrl}`
        ],
      ];
      let promises = [];
      testCases.forEach(testCase => {
        promises.push(new Promise(resolve => {
          const [title, isOk, url] = testCase;
          const img = document.createElement('img');
          document.body.appendChild(img);
          img.onload = () => isOk ? resolve() : chrome.test.fail(title);
          img.onerror = () => isOk ? chrome.test.fail(title) : resolve();
          img.src = url;
        }));
      });
      Promise.all(promises).then(() => chrome.test.succeed());
    },

    // Verify that the requested icon size is returned.
    function cachedResolutions() {
      const pixelsArr = [16, 32, 64, 48];
      const promises = pixelsArr.map(pixels => {
        return new Promise((resolve, reject) => {
          const url = `_favicon/?pageUrl=${visitedPageUrl}&size=${pixels}`;
          const image = new Image();
          image.src = url;
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
    function defaultSize() {
      const expected = 16;
      const url = `_favicon/?pageUrl=${visitedPageUrl}`;
      const image = new Image();
      image.src = url;
      image.onload = () => {
        chrome.test.assertEq(expected, image.height);
        chrome.test.assertEq(expected, image.width);
        chrome.test.succeed();
      }
    },

    // Verify uncached default icon resolutions. Supported sizes: 16, 32, 64.
    function uncachedDefaultResolutions() {
      const pixelsAndExpected =
          [[30, 16], [48, 32], [128, 64], [32, 32], [10, 16]];
      const promises = pixelsAndExpected.map(requestAndExpect => {
        return new Promise((resolve, reject) => {
          const [pixels, expected] = requestAndExpect;
          const url = `_favicon/?pageUrl=http://www.unvisited.com:${
              port}/extensions/favicon/test_file.html&size=${pixels}`
          const image = new Image();
          image.src = url;
          image.onload = () => {
            chrome.test.assertEq(expected, image.height);
            chrome.test.assertEq(expected, image.width);
            resolve();
          }
        });
      });
      Promise.all(promises).then(() => chrome.test.succeed());
    },

    // We can't use the page's favicon file
    // (chrome/test/data/extensions/favicon/favicon.ico) directly because .ico
    // files appear to consistently show up as a blank canvas, possibly due to
    // some of the graphics stack being stubbed out in browser tests. The bmp
    // here is a direct conversion of the .ico file.
    function bmpMatch() {
      const urls = [
        '_test_resources/favicon/favicon.bmp',
        `_favicon/?pageUrl=${visitedPageUrl}&size=48`
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
