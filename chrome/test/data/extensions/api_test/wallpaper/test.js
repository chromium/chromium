// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;

chrome.test.getConfig(function(config) {

  var baseURL = "http://a.com:" + config.testServer.port +
      "/extensions/api_test/wallpaper/";

  var crosapiUnavailable = false;
  if (config.customArg && config.customArg === "crosapi_unavailable") {
    crosapiUnavailable = true;
  }

  /*
   * Calls chrome.wallpaper.setWallpaper using an arraybuffer.
   * @param {string} filePath An extension relative file path.
   */
  var testSetWallpaperFromArrayBuffer = function (filePath, wantThumbnail) {
    // Loads an extension local file to an arraybuffer.
    var url = chrome.runtime.getURL(filePath);
    var wallpaperRequest = new XMLHttpRequest();
    wallpaperRequest.open('GET', url, true);
    wallpaperRequest.responseType = 'arraybuffer';

    var callback;
    if (crosapiUnavailable) {
      callback = function() {
        chrome.test.assertLastError("Unsupported ChromeOS version.");
        chrome.test.succeed();
      };
    } else if (wantThumbnail) {
      callback = function(thumbnail) {
        chrome.test.assertNe(undefined, thumbnail);
        var buffer = new Uint8Array(thumbnail);
        chrome.test.assertTrue(buffer.length > 0);
        chrome.test.succeed("setWallpaper replied successfully.");
      };
    } else {
      callback = function() {
        chrome.test.succeed("setWallpaper replied successfully.");
      };
    }

    try {
      wallpaperRequest.onloadend = function(e) {
        if (wallpaperRequest.status === 200) {
          chrome.wallpaper.setWallpaper(
              {'data': wallpaperRequest.response,
               'layout': 'CENTER_CROPPED',
               'filename': 'test',
               'thumbnail': wantThumbnail},
               callback);
        } else {
          chrome.test.fail('Failed to load local file: ' + filePath + '.');
        }
      };
      wallpaperRequest.send(null);
    } catch (e) {
      console.error(e);
      chrome.test.fail('An error thrown when requesting wallpaper.');
    }
  };

  var testSetWallpaperFromURL = function (relativeURL) {
    var url = baseURL + relativeURL;

    var callback;
    if (crosapiUnavailable) {
      callback = function() {
        chrome.test.assertLastError("Unsupported ChromeOS version.");
        chrome.test.succeed();
      };
    } else {
      callback = function() {
        chrome.test.succeed("setWallpaper replied successfully.");
      };
    }

    chrome.wallpaper.setWallpaper(
        {'url': url,
         'layout': 'CENTER_CROPPED',
         'filename': 'test'},
         callback);
  };

  chrome.test.runTests([
    function setJpgWallpaperFromAppLocalFile() {
      testSetWallpaperFromArrayBuffer('test.jpg');
    },
    function setPngWallpaperFromAppLocalFile() {
      testSetWallpaperFromArrayBuffer('test.png');
    },
    function setJpgWallpaperFromURL () {
      testSetWallpaperFromURL('test.jpg');
    },
    function setPngWallpaperFromURL () {
      testSetWallpaperFromURL('test.png');
    },
    function setNoExistingWallpaperFromURL () {
      // test1.jpg doesn't exist. Expect a 404 error.
      var expectedError =
          'Downloading wallpaper test1.jpg failed. The response code is 404.';
      chrome.wallpaper.setWallpaper(
          {'url': baseURL + 'test1.jpg',
           'layout': 'CENTER_CROPPED',
           'filename': 'test'},
           fail(expectedError));
    },
    function newRequestCancelPreviousRequest() {
      if (crosapiUnavailable) {
        chrome.test.succeed("skipped.");
        return;
      }

      // The first request should be canceled. The wallpaper in the first
      // request is chosen from one of the high-resolution built-in wallpapers
      // to make sure the first setWallpaper request hasn't finished yet when
      // the second request sends out.

      chrome.wallpaper.setWallpaper(
          {'url': baseURL + 'test_image_high_resolution.jpg',
           'layout': 'CENTER_CROPPED',
           'filename': 'test'},
           fail('Set wallpaper was canceled.'));

      chrome.wallpaper.setWallpaper(
          {'url': baseURL + 'test.jpg',
           'layout': 'CENTER_CROPPED',
           'filename': 'test'},
           pass());

    },
    function getThumbnailAferSetWallpaper() {
      testSetWallpaperFromArrayBuffer('test.jpg', /*wantThumbnail=*/true);
    }
  ]);
});
