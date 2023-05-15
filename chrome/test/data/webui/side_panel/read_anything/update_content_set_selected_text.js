// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.UpdateContent_SetSelectedText

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readAnything.onConnected = () => {};

  const readAnythingApp =
      document.querySelector('read-anything-app').shadowRoot;
  const container = readAnythingApp.getElementById('container');
  let result = true;

  const assertEquals = (actual, expected) => {
    const isEqual = actual === expected;
    if (!isEqual) {
      console.error(
          'Expected: ' + JSON.stringify(expected) + ', ' +
          'Actual: ' + JSON.stringify(actual));
    }
    result = result && isEqual;
    return isEqual;
  };

  const assertContainerInnerHTML = (expected) => {
    const actual = container.innerHTML;
    assertEquals(actual, expected);
  };

  const setOnSelectionChangeForTest = () => {
    // This is called by readAnythingApp onselectionchange. It is usually
    // implemented by ReadAnythingAppController which forwards these
    // arguments to the browser process in the form of an
    // AXEventNotificationDetail. Instead, we capture the arguments here and
    // verify their values. Since onselectionchange is called
    // asynchronously, the test must wait for this function to be called;
    // therefore we fire a custom event on-selection-change-for-text here
    // for the test to await.
    chrome.readAnything.onSelectionChange =
        (anchorNodeId, anchorOffset, focusNodeId, focusOffset) => {
          readAnythingApp.dispatchEvent(
              new CustomEvent('on-selection-change-for-test', {
                detail: {
                  anchorNodeId: anchorNodeId,
                  anchorOffset: anchorOffset,
                  focusNodeId: focusNodeId,
                  focusOffset: focusOffset,
                },
              }));
        };
  };

  // root htmlTag='#document' id=1
  // ++staticText name='Hello' id=2
  // ++staticText name='World' id=3
  // ++staticText name='Friend' id=4
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2, 3, 4],
      },
      {
        id: 2,
        role: 'staticText',
        name: 'Hello',
      },
      {
        id: 3,
        role: 'staticText',
        name: 'World',
      },
      {
        id: 4,
        role: 'staticText',
        name: 'Friend',
      },
    ],
  };
  setOnSelectionChangeForTest();
  chrome.readAnything.setContentForTesting(axTree, [1]);
  const expected = '<div>HelloWorldFriend</div>';
  assertContainerInnerHTML(expected);

  // When the selection is set, readAnythingApp listens for the selection
  // change event and calls chrome.readAnything.onSelectionChange. This test
  // overrides that method and fires a custom event
  // 'on-selection-change-for-test' with the parameters to onSelectionChange
  // stored in details. Here, we check the values of the parameters of
  // onSelectionChange.
  const resultPromise = new Promise(resolve => {
    readAnythingApp.addEventListener(
        'on-selection-change-for-test', (onSelectionChangeEvent) => {
          assertEquals(onSelectionChangeEvent.detail.anchorNodeId, 2);
          assertEquals(onSelectionChangeEvent.detail.anchorOffset, 1);
          assertEquals(onSelectionChangeEvent.detail.focusNodeId, 4);
          assertEquals(onSelectionChangeEvent.detail.focusOffset, 2);

          resolve(result);
        });
  });

  // Create a selection of "elloWorldFr". The anchor node has id 2 and the
  // focus node has id 4.
  const outerDiv = container.firstElementChild;
  const range = new Range();
  range.setStart(outerDiv.firstChild, 1);
  range.setEnd(outerDiv.lastChild, 2);
  const selection = readAnythingApp.getSelection();
  selection.removeAllRanges();
  selection.addRange(range);

  return resultPromise;
})();
