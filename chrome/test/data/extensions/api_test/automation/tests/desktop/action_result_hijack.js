// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const allTests = [async function testActionResultHijack() {
  // Find the text field inside the Victim tab's tree,
  // waiting for the tree to load if necessary using listeners (no polling).
  const textField = await new Promise(resolve => {
    const isVictimRoot = (node) => {
      return node.role === RoleType.ROOT_WEB_AREA &&
             node.url && node.url.indexOf('Victim') >= 0;
    };

    const listener = (event) => {
      if (isVictimRoot(event.target)) {
        rootNode.removeEventListener('loadComplete', listener);
        // Found the root! Now find the text field inside it.
        const tf = event.target.find({role: RoleType.TEXT_FIELD});
        resolve(tf);
      }
    };
    rootNode.addEventListener('loadComplete', listener);

    // Check if already loaded.
    const victimRoot = rootNode.find(isVictimRoot);
    if (victimRoot) {
      const tf = victimRoot.find({role: RoleType.TEXT_FIELD});
      if (tf) {
        rootNode.removeEventListener('loadComplete', listener);
        resolve(tf);
      }
    }
  });

  assertTrue(!!textField);

  // Perform scrollBackward.
  textField.scrollBackward(result => {
    // If the fix is correct, the forged event (carrying result = false on the
    // attacker tree) will be ignored, and only the real event (carrying
    // result = true on the victim tree) will resolve this callback.
    // If vulnerable, the forged event will hijack the callback and we will
    // get 'false' here, causing the assertion to fail.
    assertTrue(result);
    chrome.test.succeed();
  });
}];

setUpAndRunDesktopTests(allTests);
