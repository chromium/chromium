// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const html = `<!doctype html>
    <html><script>
      document.addEventListener("visibilitychange", () => {
          console.log(document.visibilityState);
      });

      const input = document.createElement('input');
      document.body.appendChild(input);
      input.focus();
    </script></html>
  `;

  const {session, dp} = await testRunner.startHTML(
      html, `Tests browser window minimize, restore and focus.`);

  await dp.Runtime.enable();

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

  async function logWindowState(text, windowId) {
    const {result: {bounds}} = await dp.Browser.getWindowBounds({windowId});
    const visibilityState = await session.evaluate(`document.visibilityState`);
    const hasFocus = await session.evaluate(`document.hasFocus()`);
    testRunner.log(`${text}: ${bounds.windowState} ${
        visibilityState} hasFocus=${hasFocus}`);
  }

  const {result: {windowId}} = await dp.Browser.getWindowForTarget();

  await logWindowState('Initial', windowId);

  await dp.Browser.setWindowBounds(
      {windowId, bounds: {windowState: 'minimized'}});
  await waitForWindowState('hidden', false);
  await logWindowState('Minimized', windowId);

  await dp.Browser.setWindowBounds({windowId, bounds: {windowState: 'normal'}});
  await waitForWindowState('visible', true);
  await logWindowState('Restored', windowId);

  testRunner.completeTest();
});
