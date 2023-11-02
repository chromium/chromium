// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

config.IS_CHROME_TEST = true;
// No TestServer.
config.IS_JS_ONLY_GUEST = true;
config.TEST_DIR = 'screen_coordinates';

var screenCoordinatesTests = {};

/**
 * @private
 * @param {Number} mn Expected range's min value.
 * @param {Number} mx Expected range's max value.
 * @param {Number} a Actual value.
 */
var assertCorrectCoordinateValue_ = function(mn, mx, a, msg) {
  chrome.test.assertTrue(
      a >= mn && a <= mx,
      'Actual value [' + a + '] is not within interval ' +
      '[' + mn + ', ' + mx + ']');
};

var run = function() {
  var container = document.createElement('div');
  container.id = 'webview-tag-container';
  document.body.appendChild(container);

  utils.injectCss(config.TEST_DIR + '/' + 'style.css');

  chrome.test.getConfig(function(chromeConfig) {
    window.console.log('getConfig: ' + chromeConfig);
    utils.setUp(chromeConfig, config);
    embedder.loadGuest(function(webview) {
      window.console.log('bootstrap got embedder.loadGuest');
      chrome.test.runTests([
        function testScreenCoordinates() {
          LOG('start sending postMessage');
          webview.contentWindow.postMessage(
              JSON.stringify(['' + 'test1', 'get-screen-info']), '*');
        }
      ]);
    }, function(data) {
      // TODO: Better refactor for readability (e.g. allow callers to specify
      // matched request/response handler pair.
      if (data[0] == 'test1') {
        assertCorrectCoordinateValue_(
            window.screenX, window.screenX + window.innerWidth,
            data[1].screenX, 'screenX');
        assertCorrectCoordinateValue_(
            window.screenY, window.screenY + window.innerHeight,
            data[1].screenY, 'screenY');
        assertCorrectCoordinateValue_(
            window.screenLeft, window.screenLeft + window.innerWidth,
            data[1].screenLeft, 'screenLeft');
        assertCorrectCoordinateValue_(
            window.screenTop, window.screenTop + window.innerHeight,
            data[1].screenTop, 'screenTop');
        chrome.test.succeed();
        return true;
      }
      return /* handled */ false;
    });
  });
};

run();
