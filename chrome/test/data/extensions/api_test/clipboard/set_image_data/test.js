// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test clipboard extension api chrome.clipboard.onClipboardDataChanged event.

var testSuccessCount = 0;

function verifySetImageDataResult(expectedError) {
  if (expectedError)
    chrome.test.assertLastError(expectedError);
  chrome.test.succeed();
}

function testSetImageDataClipboard(
    imageUrl, imageType, expectedError, additionalItems) {
  var oReq = new XMLHttpRequest();
  oReq.open('GET', imageUrl, true);
  oReq.responseType = 'arraybuffer';

  oReq.onload = function (oEvent) {
    var arrayBuffer = oReq.response;
    var binaryString = '';

    if (arrayBuffer) {
      if (additionalItems) {
        chrome.clipboard.setImageData(arrayBuffer, imageType, additionalItems,
                                      function() {
          verifySetImageDataResult(expectedError);
        });
      } else {
        chrome.clipboard.setImageData(arrayBuffer, imageType,
                                      function() {
          verifySetImageDataResult(expectedError);
        });
      }
    } else {
      chrome.test.fail('Failed to load the image file');
    }
  };

  oReq.send(null);
}

function testSavePngImageToClipboard(baseUrl) {
  testSetImageDataClipboard(baseUrl + '/icon1.png', 'png');
}

function testSaveJpegImageToClipboard(baseUrl) {
  testSetImageDataClipboard(baseUrl + '/test.jpg', 'jpeg');
}

function testSaveBadImageData(baseUrl) {
  testSetImageDataClipboard(
      baseUrl + '/test_file.txt', 'jpeg', 'Image data decoding failed.');
}

function testSavePngImageWithAdditionalDataToClipboard(baseUrl) {
  var additional_items = [];
  var text_item = {
      type: 'textPlain',
      data: 'Hello, world'
  }
  var html_item = {
      type: 'textHtml',
      data: '<b>This is an html markup</b>'
  }
  additional_items.push(text_item);
  additional_items.push(html_item);
  testSetImageDataClipboard(
      baseUrl + '/icon1.png', 'png', undefined, additional_items);
}

function testSavePngImageWithAdditionalDataToClipboardDuplicateTypeItems(
    baseUrl) {
  var additional_items = [];
  var text_item1 = {
      type: 'textPlain',
      data: 'Hello, world'
  }
  var text_item2 = {
      type: 'textPlain',
      data: 'Another text item'
  }
  additional_items.push(text_item1);
  additional_items.push(text_item2);
  testSetImageDataClipboard(
      baseUrl + '/icon1.png', 'png',
      'Unsupported additionalItems parameter data.',
      additional_items);
}

function bindTest(test, param) {
  var result = test.bind(null, param);
  result.generatedName = test.name;
  return result;
}

chrome.test.getConfig(function(config) {
  var baseUrl = 'http://localhost:' + config.testServer.port + '/extensions';
  chrome.test.runTests([
    bindTest(testSavePngImageToClipboard, baseUrl),
    bindTest(testSaveJpegImageToClipboard, baseUrl),
    bindTest(testSaveBadImageData, baseUrl),
    bindTest(testSavePngImageWithAdditionalDataToClipboard, baseUrl),
    bindTest(testSavePngImageWithAdditionalDataToClipboardDuplicateTypeItems,
             baseUrl)
  ]);
})
