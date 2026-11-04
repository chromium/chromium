// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
    <a href="file:///foo/bar">Drag Me!</a>
  `,
      `Tests that disallowed URLs are filtered when dragging.`);

  await dp.Input.setInterceptDrags({enabled: true});

  const dragInterceptedPromise = dp.Input.onceDragIntercepted();

  await dp.Input.dispatchMouseEvent({
    type: 'mousePressed',
    button: 'left',
    buttons: 1,
    clickCount: 1,
    x: 20,
    y: 20,
  });
  await dp.Input.dispatchMouseEvent({
    type: 'mouseMoved',
    button: 'left',
    buttons: 1,
    clickCount: 1,
    x: 150,
    y: 150,
  });

  const result = await dragInterceptedPromise;
  testRunner.log(result.params.data, 'dragData: ');

  testRunner.completeTest();
});
