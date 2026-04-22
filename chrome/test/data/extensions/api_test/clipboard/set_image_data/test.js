// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test clipboard extension api chrome.clipboard.onClipboardDataChanged event.

const testSuccessCount = 0;

function verifySetImageDataResult(expectedError) {
  if (expectedError) {
    chrome.test.assertLastError(expectedError);
  }
  chrome.test.succeed();
}

function testSetImageDataClipboard(
    imageUrl, imageType, expectedError, additionalItems) {
  const oReq = new XMLHttpRequest();
  oReq.open('GET', imageUrl, true);
  oReq.responseType = 'arraybuffer';

  oReq.onload = function(oEvent) {
    const arrayBuffer = oReq.response;

    if (arrayBuffer) {
      if (additionalItems) {
        chrome.clipboard.setImageData(
            arrayBuffer, imageType, additionalItems, function() {
              verifySetImageDataResult(expectedError);
            });
      } else {
        chrome.clipboard.setImageData(arrayBuffer, imageType, function() {
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
  testSetImageDataClipboard(`${baseUrl}/icon1.png`, 'png');
}

function testSaveJpegImageToClipboard(baseUrl) {
  testSetImageDataClipboard(`${baseUrl}/test.jpg`, 'jpeg');
}

function testSaveBadImageData(baseUrl) {
  testSetImageDataClipboard(
      `${baseUrl}/test_file.txt`, 'jpeg', 'Image data decoding failed.');
}

function testSavePngImageWithAdditionalDataToClipboard(baseUrl) {
  const additionalItems = [];
  const textItem = {
    type: 'textPlain',
    data: 'Hello, world',
  };
  const htmlItem = {
    type: 'textHtml',
    data: '<b>This is an html markup</b>',
  };
  additionalItems.push(textItem);
  additionalItems.push(htmlItem);
  testSetImageDataClipboard(
      `${baseUrl}/icon1.png`, 'png', undefined, additionalItems);
}

function testSavePngImageWithAdditionalDataToClipboardDuplicateTypeItems(
    baseUrl) {
  const additionalItems = [];
  const textItem1 = {
    type: 'textPlain',
    data: 'Hello, world',
  };
  const textItem2 = {
    type: 'textPlain',
    data: 'Another text item',
  };
  additionalItems.push(textItem1);
  additionalItems.push(textItem2);
  testSetImageDataClipboard(
      `${baseUrl}/icon1.png`, 'png',
      'Unsupported additionalItems parameter data.', additionalItems);
}

function bindTest(test, param) {
  const result = test.bind(null, param);
  result.generatedName = test.name;
  return result;
}

chrome.test.getConfig(function(config) {
  const baseUrl = `http://localhost:${config.testServer.port}/extensions`;
  chrome.test.runTests([
    bindTest(testSavePngImageToClipboard, baseUrl),
    bindTest(testSaveJpegImageToClipboard, baseUrl),
    bindTest(testSaveBadImageData, baseUrl),
    bindTest(testSavePngImageWithAdditionalDataToClipboard, baseUrl),
    bindTest(
        testSavePngImageWithAdditionalDataToClipboardDuplicateTypeItems,
        baseUrl),
  ]);
});
