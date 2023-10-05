// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

function assertNativeUIButtonDisabled(disabled: boolean) {
  const button =
      document.querySelector<HTMLButtonElement>('#launch-ui-devtools');
  assertTrue(!!button);
  assertEquals(disabled, button.disabled);
}

// This function is called directly from C++, so export it on |window|.
Object.assign(window, {assertNativeUIButtonDisabled});

suite('InspectUITest', function() {
  function waitForElements(
      selector: string, populateFunctionName: string): Promise<Element[]> {
    const elements = document.querySelectorAll(selector);
    if (elements.length) {
      return Promise.resolve(Array.from(elements));
    }
    const windowObj = window as unknown as {[key: string]: Function};
    const originalFunction = windowObj[populateFunctionName]!;
    assertTrue(!!originalFunction);
    assertEquals(
        undefined,
        (originalFunction as Function & {__isSniffer?: boolean}).__isSniffer);
    return new Promise(resolve => {
      const interceptFunction = function() {
        originalFunction.apply(window, arguments);
        const elements = document.querySelectorAll(selector);
        if (elements.length) {
          windowObj[populateFunctionName] = originalFunction;
          resolve(Array.from(elements));
        }
      };
      interceptFunction.__isSniffer = true;
      windowObj[populateFunctionName] = interceptFunction;
    });
  }

  function findByContentSubstring(
      elements: Element[], content: string, childSelector: string) {
    return elements.find(element => {
      const child = element.querySelector(childSelector);
      return !!child && child.textContent!.indexOf(content) >= 0;
    });
  }

  async function testTargetListed(
      sectionSelector: string, populateFunctionName: string, url: string) {
    const elements =
        await waitForElements(sectionSelector + ' .row', populateFunctionName);
    const urlElement = findByContentSubstring(elements, url, '.url');
    assertNotEquals(undefined, urlElement);
  }

  test('InspectUIPage', async () => {
    await testTargetListed(
        '#pages', 'populateWebContentsTargets', 'chrome://inspect');
  });

  test('SharedWorker', async () => {
    await testTargetListed(
        '#workers', 'populateWorkerTargets',
        '/workers/workers_ui_shared_worker.js');
    await testTargetListed(
        '#pages', 'populateWebContentsTargets',
        '/workers/workers_ui_shared_worker.html');
  });

  test('SharedStorageWorklet', async () => {
    await testTargetListed(
        '#shared-storage-worklets', 'populateWorkerTargets',
        '/shared_storage/simple_module.js');
  });

  test('Empty', function() {
    // Intentionally empty test, for the browser test that calls
    // assertNativeButtonDisabled directly.
  });


  test('AdbTargetsListed', async () => {
    const devices = await waitForElements('.device', 'populateRemoteTargets');
    assertEquals(2, devices.length);

    const offlineDevice =
        findByContentSubstring(devices, 'Offline', '.device-name');
    assertTrue(!!offlineDevice);

    const onlineDevice =
        findByContentSubstring(devices, 'Nexus 6', '.device-name');
    assertTrue(!!onlineDevice);

    const browsers = Array.from(onlineDevice.querySelectorAll('.browser'));
    assertEquals(4, browsers.length);

    const chromeBrowser = findByContentSubstring(
        browsers, 'Chrome (32.0.1679.0)', '.browser-name');
    assertTrue(!!chromeBrowser);

    const chromePages = Array.from(chromeBrowser.querySelectorAll('.pages'));
    const chromiumPage =
        findByContentSubstring(chromePages, 'http://www.chromium.org/', '.url');
    assertNotEquals(undefined, chromiumPage);

    const pageById = {} as {[key: string]: Element};
    devices.forEach(device => {
      const pages = Array.from(device.querySelectorAll('.row'));
      pages.forEach(page => {
        const targetId =
            (page as unknown as Element & {targetId: string}).targetId;
        assertEquals(undefined, pageById[targetId]);
        pageById[targetId] = page;
      });
    });

    const webView = findByContentSubstring(
        browsers, 'WebView in com.sample.feed (4.0)', '.browser-name');
    assertNotEquals(undefined, webView);
  });
});
