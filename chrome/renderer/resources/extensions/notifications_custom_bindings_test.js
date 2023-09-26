// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function assertTrue(condition) {
  if (!condition) {
    throw new Error('Assertion failed: expected ' + condition + ' to be true');
  }
}

function assertEquals(a, b) {
  if (a !== b) {
    throw new Error('Assertion failed: expected ' + a + ' to equal ' + b);
  }
}

function assertFalse(condition) {
  if (!!condition) {
    throw new Error('Assertion failed: expected ' + condition + ' to be false');
  }
}

function testImageDataSetter() {
  var c = {};
  var k = "key";
  var callback = imageDataSetter(c, k);
  callback('val');
  assertTrue(c[k] === 'val');
}

function testGetUrlSpecs() {
  var imageSizes = {
    scaleFactor: 1.0,
    icon: { width: 10, height: 10 },
    image: { width: 24, height: 32 },
    buttonIcon: { width: 2, height: 2}
  };

  var notificationDetails = {};

  var emptySpecs = getUrlSpecs(imageSizes, notificationDetails);
  assertTrue(emptySpecs.length === 0);

  notificationDetails.iconUrl = "iconUrl";
  notificationDetails.imageUrl = "imageUrl";
  notificationDetails.buttons = [
    {iconUrl: "buttonOneIconUrl"},
    {iconUrl: "buttonTwoIconUrl"}];

  var allSpecs = getUrlSpecs(imageSizes, notificationDetails);
  assertTrue(allSpecs.length === 4);

  assertFalse(notificationDetails.iconBitmap === "test");
  assertFalse(notificationDetails.imageBitmap === "test");
  assertFalse(notificationDetails.buttons[0].iconBitmap === "test");
  assertFalse(notificationDetails.buttons[1].iconBitmap === "test");

  for (var i = 0; i < allSpecs.length; i++) {
    var expectedKeys = ['path', 'width', 'height', 'callback'];
    var spec = allSpecs[i];
    for (var j in expectedKeys) {
      assertTrue(spec.hasOwnProperty(expectedKeys[j]));
    }
    spec.callback(spec.path + "|" + spec.width + "|" + spec.height);
  }

  assertTrue(notificationDetails.iconBitmap === "iconUrl|10|10");
  assertTrue(notificationDetails.imageBitmap === "imageUrl|24|32");
  assertTrue(
      notificationDetails.buttons[0].iconBitmap === "buttonOneIconUrl|2|2");
  assertTrue(
      notificationDetails.buttons[1].iconBitmap === "buttonTwoIconUrl|2|2");
}

function testGetUrlSpecsScaled() {
  var imageSizes = {
    scaleFactor: 2.0,
    icon: { width: 10, height: 10 },
    image: { width: 24, height: 32 },
    buttonIcon: { width: 2, height: 2}
  };
  var notificationDetails = {
    iconUrl: "iconUrl",
    imageUrl: "imageUrl",
    buttons: [
      {iconUrl: "buttonOneIconUrl"},
      {iconUrl: "buttonTwoIconUrl"}
    ]
  };

  var allSpecs = getUrlSpecs(imageSizes, notificationDetails);
  for (var i = 0; i < allSpecs.length; i++) {
    var spec = allSpecs[i];
    spec.callback(spec.path + "|" + spec.width + "|" + spec.height);
  }

  assertEquals(notificationDetails.iconBitmap, "iconUrl|20|20");
  assertEquals(notificationDetails.imageBitmap, "imageUrl|48|64");
  assertEquals(notificationDetails.buttons[0].iconBitmap,
               "buttonOneIconUrl|4|4");
  assertEquals(notificationDetails.buttons[1].iconBitmap,
               "buttonTwoIconUrl|4|4");
}
