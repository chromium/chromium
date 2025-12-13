// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests Target.createTarget with new browser context');

  const {browserContextId} = await dp.Target.createBrowserContext();
  const {targetId} = await dp.Target.createTarget({
    url: 'data:text/html,<!DOCTYPE html>',
    browserContextId,
    newWindow: true,
  });
  const {result} = await dp.Target.attachToTarget({targetId, flatten: true});
  await dp.Target.disposeBrowserContext({browserContextId});
  testRunner.log('Test ran to completion');
  testRunner.completeTest();
});
