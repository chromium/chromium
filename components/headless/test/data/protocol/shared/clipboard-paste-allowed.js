// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests browser-side clipboard paste allowed check.');

  async function waitForWindowState(expectedVisibility, expectedFocus) {
    return await session.evaluateAsync(async (expectedVis, expectedFoc) => {
      if (document.visibilityState === expectedVis &&
          document.hasFocus() === expectedFoc) {
        return;
      }
      return await new Promise(resolve => {
        const check = () => {
          if (document.visibilityState === expectedVis &&
              document.hasFocus() === expectedFoc) {
            document.removeEventListener('visibilitychange', check);
            window.removeEventListener('focus', check);
            window.removeEventListener('blur', check);
            resolve();
          }
        };
        document.addEventListener('visibilitychange', check);
        window.addEventListener('focus', check);
        window.addEventListener('blur', check);
      });
    }, expectedVisibility, expectedFocus);
  }

  // Grant permission and pre-create lazy ClipboardItem instances while focused.
  // Calling getType('text/plain') on a lazy ClipboardItem bypasses renderer
  // permission/focus checks and goes straight to ClipboardHostImpl::ReadText,
  // exercising ContentBrowserClient::IsClipboardPasteAllowed() in the browser.
  await dp.Browser.grantPermissions({permissions: ['clipboardReadWrite']});
  await session.evaluateAsync(async () => {
    await navigator.clipboard.writeText('clipboard_text');
    window.clipboardItems = [];
    for (let i = 0; i < 4; ++i) {
      const [item] = await navigator.clipboard.read();
      window.clipboardItems.push(item);
    }
  });

  async function readClipboardItem(label) {
    const text = await session.evaluateAsync(async () => {
      const item = window.clipboardItems.shift();
      const blob = await item.getType('text/plain');
      return await blob.text();
    });
    testRunner.log(`${label}: "${text}"`);
  }

  await readClipboardItem('Focused with permission');

  const {result: {windowId}} = await dp.Browser.getWindowForTarget();
  await dp.Browser.setWindowBounds(
      {windowId, bounds: {windowState: 'minimized'}});
  await waitForWindowState('hidden', false);
  await readClipboardItem('Unfocused with permission');

  await dp.Browser.setWindowBounds({windowId, bounds: {windowState: 'normal'}});
  await waitForWindowState('visible', true);
  await dp.Browser.resetPermissions();
  await readClipboardItem('Focused without permission or interaction');

  await dp.Input.dispatchKeyEvent({type: 'keyDown', text: 'a'});
  await dp.Input.dispatchKeyEvent({type: 'keyUp'});
  await readClipboardItem('Focused with recent user interaction');

  testRunner.completeTest();
});
