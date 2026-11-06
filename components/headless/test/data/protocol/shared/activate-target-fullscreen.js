// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --enable-features=SuppressFullscreenFromUnfocusedView

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  await testRunner.startBlank(
      'Tests fullscreen after activating a background target.');

  const bp = testRunner.browserP();
  const {targetId} =
      (await bp.Target.createTarget({url: 'about:blank', background: true}))
          .result;
  const session = await testRunner.browserSession().attachChild(targetId);

  await bp.Target.activateTarget({targetId});
  const result = await session.evaluateAsyncWithUserGesture(() => {
    return document.body.requestFullscreen().then(
        () => document.fullscreenElement === document.body,
        error => error.toString());
  });
  testRunner.log('Fullscreen after target activation: ' + result);

  await bp.Target.closeTarget({targetId});
  testRunner.completeTest();
});
