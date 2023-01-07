// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function waitForElements(selector, populateFunctionName, callback) {
  const elements = document.querySelectorAll(selector);
  if (elements.length) {
    callback(elements);
    return;
  }
  const originalFunction = window[populateFunctionName];
  assertNotEquals(undefined, originalFunction);
  assertEquals(undefined, originalFunction.__isSniffer);
  const interceptFunction = function() {
    originalFunction.apply(window, arguments);
    const elements = document.querySelectorAll(selector);
    if (elements.length) {
      window[populateFunctionName] = originalFunction;
      callback(elements);
    }
  };
  interceptFunction.__isSniffer = true;
  window[populateFunctionName] = interceptFunction;
}

function findByContentSubstring(elements, content, childSelector) {
  return Array.prototype.filter.call(elements, function(element) {
    if (childSelector) {
      element = element.querySelector(childSelector);
    }
    return element && element.textContent.indexOf(content) >= 0;
  })[0];
}

function testTargetListed(sectionSelector, populateFunctionName, url) {
  waitForElements(
      sectionSelector + ' .row', populateFunctionName, function(elements) {
        const urlElement = findByContentSubstring(elements, url, '.url');
        assertNotEquals(undefined, urlElement);
        testDone();
      });
}

function testAdbTargetsListed() {
  waitForElements('.device', 'populateRemoteTargets', function(devices) {
    assertEquals(2, devices.length);

    const offlineDevice =
        findByContentSubstring(devices, 'Offline', '.device-name');
    assertNotEquals(undefined, offlineDevice);

    const onlineDevice =
        findByContentSubstring(devices, 'Nexus 6', '.device-name');
    assertNotEquals(undefined, onlineDevice);

    const browsers = onlineDevice.querySelectorAll('.browser');
    assertEquals(4, browsers.length);

    const chromeBrowser = findByContentSubstring(
        browsers, 'Chrome (32.0.1679.0)', '.browser-name');
    assertNotEquals(undefined, chromeBrowser);

    const chromePages = chromeBrowser.querySelectorAll('.pages');
    const chromiumPage =
        findByContentSubstring(chromePages, 'http://www.chromium.org/', '.url');
    assertNotEquals(undefined, chromiumPage);

    const pageById = {};
    Array.prototype.forEach.call(devices, function(device) {
      const pages = device.querySelectorAll('.row');
      Array.prototype.forEach.call(pages, function(page) {
        assertEquals(undefined, pageById[page.targetId]);
        pageById[page.targetId] = page;
      });
    });

    const webView = findByContentSubstring(
        browsers, 'WebView in com.sample.feed (4.0)', '.browser-name');
    assertNotEquals(undefined, webView);

    testDone();
  });
}

function testElementDisabled(selector, disabled) {
  const element = document.querySelector(selector);
  assertEquals(disabled, element.disabled);
  testDone();
}
