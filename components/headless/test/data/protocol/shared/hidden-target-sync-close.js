// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} =
      await testRunner.startBlank(`Test hidden targets synchronous close`);

  const {result: {browserContextId}} =
      await testRunner.browserP().Target.createBrowserContext();

  const {result: mainTarget} = await testRunner.browserP().Target.createTarget({
    url: testRunner.url('/resources/blank.html'),
    browserContextId,
  });

  const attachedToMainTarget =
      await testRunner.browserP().Target.attachToTarget({
        targetId: mainTarget.targetId,
        flatten: true,
      });
  const mainSession =
      session.createChild(attachedToMainTarget.result.sessionId);

  const hiddenTargetIds = [];
  for (let i = 0; i < 20; ++i) {
    const {result: {targetId: hiddenId}} =
        await mainSession.protocol.Target.createTarget({
          url: `about:blank`,
          hidden: true,
          browserContextId,
        });
    hiddenTargetIds.push(hiddenId);
  }

  const crashPromises = [];
  for (targetId of hiddenTargetIds) {
    const attachedToHiddenTarget =
        await testRunner.browserP().Target.attachToTarget({
          targetId,
          flatten: true,
        });
    const hiddenSession =
        session.createChild(attachedToHiddenTarget.result.sessionId);
    hiddenSession.protocol.Page.crash();
    crashPromises.push(hiddenSession.protocol.Inspector.onceTargetCrashed());
  }

  await Promise.all(crashPromises);
  testRunner.log(`Crashed ${crashPromises.length} targets`);
  await testRunner.browserP().Target.disposeBrowserContext({browserContextId});
  testRunner.log('PASSED');
  testRunner.completeTest();
});
