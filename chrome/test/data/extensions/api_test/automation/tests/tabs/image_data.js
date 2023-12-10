// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testGetImageData() {
    var image = rootNode.find({ role: RoleType.IMAGE });
    image.addEventListener(EventType.IMAGE_FRAME_UPDATED, function() {
      assertEq(image.imageDataUrl.substr(0, 22), 'data:image/png;base64,');
      fetch(image.imageDataUrl).then(function(response) {
        if (!response.ok) {
          throw response;
        }
        return response.blob();
      }).then(function(blob) {
        return createImageBitmap(blob);
      }).then(function(img) {
        var canvas = new OffscreenCanvas(2, 3);
        var context = canvas.getContext('2d');
        context.drawImage(img, 0, 0);
        var imageData = context.getImageData(0, 0, 2, 3);
        // Check image data in RGBA format.
        // Top row: red
        assertEq(imageData.data[0], 0xFF);
        assertEq(imageData.data[1], 0x00);
        assertEq(imageData.data[2], 0x00);
        assertEq(imageData.data[3], 0xFF);
        assertEq(imageData.data[4], 0xFF);
        assertEq(imageData.data[5], 0x00);
        assertEq(imageData.data[6], 0x00);
        assertEq(imageData.data[7], 0xFF);
        // Middle row: green
        assertEq(imageData.data[8], 0x00);
        assertEq(imageData.data[9], 0xFF);
        assertEq(imageData.data[10], 0x00);
        assertEq(imageData.data[11], 0xFF);
        assertEq(imageData.data[12], 0x00);
        assertEq(imageData.data[13], 0xFF);
        assertEq(imageData.data[14], 0x00);
        assertEq(imageData.data[15], 0xFF);
        // Last row: blue
        assertEq(imageData.data[16], 0x00);
        assertEq(imageData.data[17], 0x00);
        assertEq(imageData.data[18], 0xFF);
        assertEq(imageData.data[19], 0xFF);
        assertEq(imageData.data[20], 0x00);
        assertEq(imageData.data[21], 0x00);
        assertEq(imageData.data[22], 0xFF);
        assertEq(imageData.data[23], 0xFF);
        chrome.test.succeed();
      });
    }, true);
    image.getImageData(0, 0);
  }
];

setUpAndRunTabsTests(allTests, 'image_data.html');
