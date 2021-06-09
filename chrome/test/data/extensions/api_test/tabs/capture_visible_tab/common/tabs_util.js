// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility functions to help with tabs/windows testing.

// Creates one window with tabs set to the urls in the array |tabUrls|.
// At least one url must be specified.
// The |callback| should look like function(windowId, tabIds) {...}.
function createWindow(tabUrls, winOptions, callback) {
  winOptions["url"] = tabUrls;
  chrome.windows.create(winOptions, function(win) {
    var newTabIds = [];
    assertTrue(win.id > 0);
    assertEq(tabUrls.length, win.tabs.length);

    for (var i = 0; i < win.tabs.length; i++)
      newTabIds.push(win.tabs[i].id);

    callback(win.id, newTabIds);
  });
}

// Waits until all tabs (yes, in every window) have status "complete".
// This is useful to prevent test overlap when testing tab events.
// |callback| should look like function() {...}.
function waitForAllTabs(callback) {
  // Wait for all tabs to load.
  function waitForTabs(){
    chrome.windows.getAll({"populate": true}, function(windows) {
      var ready = true;
      for (var i in windows){
        for (var j in windows[i].tabs) {
          if (windows[i].tabs[j].status != "complete") {
            ready = false;
            break;
          }
        }
        if (!ready)
          break;
      }
      if (ready)
        callback();
      else
        window.setTimeout(waitForTabs, 30);
    });
  }
  waitForTabs();
}

// Run callbackFn() with an array of strings representing the
// color of each pixel in a small region of the image.  Strings
// representing pixels look like this: '255,255,255,0'.
function getPixels(imgUrl, windowRect, callbackFn) {
  assertEq('string', typeof(imgUrl));
  var img = new Image();
  img.width = windowRect.width;
  img.height = windowRect.height;
  img.src = imgUrl;
  img.onload = pass(function() {
    var canvas = document.createElement('canvas');

    // Comparing pixels is slow enough to hit timeouts if we run on
    // the whole image..  Compare a 10x10 region.
    canvas.setAttribute('width', 10);
    canvas.setAttribute('height', 10);
    var context = canvas.getContext('2d');
    context.drawImage(
      img,
      10, 10, 10, 10,  // Source rect: Crop to x in 10..20, y in 10..20.
      0, 0, 10, 10);   // Dest rect is 10x10.  No resizing is done.

    var imageData = context.getImageData(0, 0, 10, 10).data;

    var pixelColors = [];
    for (var i = 0, n = imageData.length; i < n; i += 4) {
      pixelColors.push([imageData[i+0],
                        imageData[i+1],
                        imageData[i+2],
                        imageData[i+3]].join(','));
    }

    callbackFn(pixelColors);
  });
}

// Check that pixels in a small region of |imgUrl| are the color
// |expectedColor|.
function testPixelsAreExpectedColor(imgUrl, windowRect, expectedColor) {
  getPixels(imgUrl, windowRect, function(pixelColors) {
    var badPixels = [];
    for (var i = 0, ie = pixelColors.length; i < ie; ++i) {
      if (pixelColors[i] != expectedColor) {
        badPixels.push({'i': i, 'color': pixelColors[i]});
      }
    }
    assertEq('[]', JSON.stringify(badPixels, null, 2));
  });
}

// Build a count of the number of times the colors in
// |expectedColors| occur in a small part of the image at |imgUrl|.
function countPixelsWithColors(imgUrl, windowRect, expectedColors, callback) {
  colorCounts = new Array(expectedColors.length);
  for (var i = 0; i < expectedColors.length; ++i) {
    colorCounts[i] = 0;
  }

  getPixels(imgUrl, windowRect, function(pixelColors) {
    for (var i = 0, ie = pixelColors.length; i < ie; ++i) {
      var colorIdx = expectedColors.indexOf(pixelColors[i]);
      if (colorIdx != -1)
        colorCounts[colorIdx]++;
    }
    callback(colorCounts,          // Mapping from color to # pixels.
             pixelColors.length);  // Total pixels examined.
  });
}

function pageUrl(base) {
  return chrome.extension.getURL('common/' + base + '.html');
}

function assertIsStringWithPrefix(prefix, str) {
  assertEq('string', typeof(str));
  assertEq(prefix, str.substr(0, prefix.length));
}
