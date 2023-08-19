// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  async function testAddRemoveOnDesktop() {
    const desktop = await new Promise(r => chrome.automation.getDesktop(r));

    // |desktop| at this point has no listeners. A shutdown of the automation
    // tree is not triggered for this case. To do so now would be too strict.
    assertEq(RoleType.DESKTOP, desktop.role);

    // Adding a listener should have no effect.
    const focusHandler = () => {};
    desktop.addEventListener(EventType.FOCUS, focusHandler);
    desktop.firstChild.addEventListener(EventType.FOCUS, focusHandler);
    assertEq(RoleType.DESKTOP, desktop.role);

    // Removing the listener, though, clears the automation tree as signified by
    // the lack of node data.
    desktop.removeEventListener(EventType.FOCUS, focusHandler);
    desktop.firstChild.removeEventListener(EventType.FOCUS, focusHandler);

    await pollUntil(() => desktop.role === undefined, 100);

    // Re-requesting desktop should just work.
    const newDesktop = await new Promise(r => chrome.automation.getDesktop(r));
    assertTrue(!!newDesktop);
    assertEq(RoleType.DESKTOP, newDesktop.role);

    chrome.test.succeed();
  },

  async function testGetDesktopWhileDisabling() {
    const desktop = await new Promise(r => chrome.automation.getDesktop(r));
    const focusHandler = () => {};
    desktop.addEventListener(EventType.FOCUS, focusHandler);

    // A disableDesktop call is sent and in-flight. Getting a new desktop should
    // queue up and work after that.
    desktop.removeEventListener(EventType.FOCUS, focusHandler);

    // This does get the current desktop as well, prior to disabling. Doing this
    // checks that repeated calls still work prior to the disable coming
    // through.
    const newDesktop = await new Promise(r => chrome.automation.getDesktop(r));

    // Both |desktop| and |newDesktop| should be valid and refer to the
    // same tree.
    assertTrue(!!desktop);
    assertTrue(!!newDesktop);
    assertEq(newDesktop, desktop);
    assertEq(RoleType.DESKTOP, newDesktop.role);

    // Finally, the disabling above (from removing the event listeners) comes
    // some time later. Wait for it so that it does not impact other tests.
    await pollUntil(() => desktop.role === undefined, 100);

    chrome.test.succeed();
  },

  async function testUnbalancedAddRemoveEventListeners() {
    const desktop = await new Promise(r => chrome.automation.getDesktop(r));
    assertTrue(!!desktop);

    // Intentionally unbalanced add to remove below.
    const handler = () => {};
    desktop.addEventListener(EventType.FOCUS, handler);
    desktop.removeEventListener(EventType.FOCUS, handler);
    desktop.addEventListener(EventType.FOCUS, handler);

    // Unfortunately, we have to introduce timing here to not manipulate the
    // very event state that is being tested.
    await new Promise(r => {
      setTimeout(() => {
        // If a disable occurred, the desktop would have been destroyed.
        assertEq(RoleType.DESKTOP, desktop.role);
        r();
      }, 100);
    });

    // Remove the event listener, and wait for the desktop to be destroyed.
    desktop.removeEventListener(EventType.FOCUS, handler);
    await pollUntil(() => desktop.role === undefined, 100);
    chrome.test.succeed();
  },

  async function testAddRemoveOnDisappearingButtons() {
    const desktop = await new Promise(r => chrome.automation.getDesktop(r));
    assertTrue(!!desktop);

    const button = await pollUntil(
        () => findAutomationNode(desktop, n => n.name === 'remove'), 100);

    assertTrue(!!button);
    assertEq('remove', button.name);

    // Adding a listener should have no effect.
    const focusHandler = () => {};
    button.addEventListener(EventType.FOCUS, focusHandler);
    assertEq(RoleType.DESKTOP, desktop.role);

    // The click/do default action triggers removal of the button.
    button.doDefault();

    // We can't add event listeners to observe the deletions as to not trigger
    // adds, so poll for the change.
    await pollUntil(() => button.role === undefined, 10);

    // The tree is completely cleared.
    assertEq(undefined, desktop.role);
    chrome.test.succeed();
  },

  // Note that these tests run on the *same* webpage, so the above test already
  // removed/hide one of the buttons.
  async function testWindowClose() {
    const desktop = await new Promise(r => chrome.automation.getDesktop(r));
    assertTrue(!!desktop);

    const button = await pollUntil(
        () => findAutomationNode(desktop, n => n.name === 'close'), 100);
    assertTrue(!!button);
    assertEq('close', button.name);

    // Adding a listener should have no effect.
    const focusHandler = () => {};
    button.addEventListener(EventType.FOCUS, focusHandler);
    assertEq(RoleType.DESKTOP, desktop.role);

    // The click/do default action triggers the window to close.
    button.doDefault();

    // We can't add event listeners to observe the deletions as to not trigger
    // adds, so poll for the change.
    await pollUntil(() => button.role === undefined, 10);

    // The tree should not be cleared.
    await new Promise(r => {
      setTimeout(() => {
        if (desktop.role !== undefined) {
          r();
        }
      }, 100);
    });

    chrome.test.succeed();
  }
];

setUpAndRunTestsInPage(
    allTests, 'add_remove_event_listeners.html', /* ensurePersists = */ false);
