// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  async function testAddRemoveOnDesktop() {
    const desktop = await new Promise(r => chrome.automation.getDesktop(r));

    // |desktop| at this point has no listeners. A shutdown of the automation
    // tree is not triggered for this case. To do so now would be too strict.
    assertEq(chrome.automation.RoleType.DESKTOP, desktop.role);

    // Adding a listener should have no effect.
    const focusHandler = () => {};
    desktop.addEventListener(chrome.automation.EventType.FOCUS, focusHandler);
    desktop.firstChild.addEventListener(
        chrome.automation.EventType.FOCUS, focusHandler);
    assertEq(chrome.automation.RoleType.DESKTOP, desktop.role);

    // Removing the listener, though, clears the automation tree as signified by
    // the lack of node data.
    desktop.removeEventListener(
        chrome.automation.EventType.FOCUS, focusHandler);
    desktop.firstChild.removeEventListener(
        chrome.automation.EventType.FOCUS, focusHandler);

    await new Promise(r => {
      setInterval(() => {
        if (desktop.role === undefined) {
          r();
        }
      }, 100);
    });

    // Re-requesting desktop should just work.
    const newDesktop = await new Promise(r => chrome.automation.getDesktop(r));
    assertTrue(!!newDesktop);
    assertEq(chrome.automation.RoleType.DESKTOP, newDesktop.role);

    chrome.test.succeed();
  },

  async function testGetDesktopWhileDisabling() {
    const desktop = await new Promise(r => chrome.automation.getDesktop(r));
    const focusHandler = () => {};
    desktop.addEventListener(chrome.automation.EventType.FOCUS, focusHandler);

    // A disableDesktop call is sent and in-flight. Getting a new desktop should
    // queue up and work after that.
    desktop.removeEventListener(
        chrome.automation.EventType.FOCUS, focusHandler);

    // This does get the current desktop as well, prior to disabling. Doing this
    // checks that repeated calls still work prior to the disable coming
    // through.
    const newDesktop = await new Promise(r => chrome.automation.getDesktop(r));

    // Both |desktop| and |newDesktop| should be valid and refer to the
    // same tree.
    assertTrue(!!desktop);
    assertTrue(!!newDesktop);
    assertEq(newDesktop, desktop);
    assertEq(chrome.automation.RoleType.DESKTOP, newDesktop.role);

    // Finally, the disabling above (from removing the event listeners) comes
    // some time later. Wait for it so that it does not impact other tests.
    await new Promise(r => {
      setInterval(() => {
        if (desktop.role === undefined) {
          r();
        }
      }, 100);
    });

    chrome.test.succeed();
  },

  async function testAddRemoveOnDisappearingButtons() {
    const desktop = await new Promise(r => chrome.automation.getDesktop(r));
    assertTrue(!!desktop);

    const button = await new Promise(r => {
      setInterval(() => {
        let node;
        if (node = findAutomationNode(desktop, n => n.name === 'remove')) {
          r(node);
        }
      }, 100);
    });
    assertTrue(!!button);
    assertEq('remove', button.name);

    // Adding a listener should have no effect.
    const focusHandler = () => {};
    button.addEventListener(chrome.automation.EventType.FOCUS, focusHandler);
    assertEq(chrome.automation.RoleType.DESKTOP, desktop.role);

    // The click/do default action triggers removal of the button.
    button.doDefault();

    // We can't add event listeners to observe the deletions as to not trigger
    // adds, so poll for the change.
    await new Promise(r => {
      const checkForButton = () => {
        if (button.role === undefined) {
          r();
          clearInterval(id);
        }
      };
      const id = setInterval(checkForButton, 10);
    });

    // The tree is completely cleared.
    assertEq(undefined, desktop.role);
    chrome.test.succeed();
  },

  // Note that these tests run on the *same* webpage, so the above test already
  // removed/hide one of the buttons.
  async function testWindowClose() {
    const desktop = await new Promise(r => chrome.automation.getDesktop(r));
    assertTrue(!!desktop);

    const button = await new Promise(r => {
      setInterval(() => {
        let node;
        if (node = findAutomationNode(desktop, n => n.name === 'close')) {
          r(node);
        }
      }, 100);
    });
    assertTrue(!!button);
    assertEq('close', button.name);

    // Adding a listener should have no effect.
    const focusHandler = () => {};
    button.addEventListener(chrome.automation.EventType.FOCUS, focusHandler);
    assertEq(chrome.automation.RoleType.DESKTOP, desktop.role);

    // The click/do default action triggers the window to close.
    button.doDefault();

    // We can't add event listeners to observe the deletions as to not trigger
    // adds, so poll for the change.
    await new Promise(r => {
      const checkForButton = () => {
        if (button.role === undefined) {
          r();
          clearInterval(id);
        }
      };
      const id = setInterval(checkForButton, 10);
    });

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

setUpAndRunTestsInPage(allTests, 'add_remove_event_listeners.html');
