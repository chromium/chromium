// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testTreeChangedObserverForCreatingNode() {
    var addButton = rootNode.find({attributes: {name: 'Add'}});
    var observerOneCallCount = 0;
    var observerTwoCallCount = 0;

    function observerOne(change) {
      if (change.type == "subtreeCreated" && change.target.name == "New") {
        observerOneCallCount++;
        // The first observer should only ever be called once and then it is
        // removed.
        chrome.test.assertEq(1, observerOneCallCount);
        // TODO(tjudkins): We need to remove the observer on a delay because the
        // underlying way we store the observers isn't actually safe to modify
        // while they are still potentially being iterated over. We should fix
        // this and support observers being able to remove themselves when
        // triggered.
        setTimeout(() => {
          chrome.automation.removeTreeChangeObserver(observerOne);
          // Trigger the button again, but this time only observerTwo should
          // receive the changes as we have removed observerOne. We trigger it
          // on a delay to ensure that the observer has finished being removed.
          setTimeout(() => {
            addButton.doDefault();
          }, 0);
        }, 0);
      }
    };
    function observerTwo(change) {
      if (change.type == 'subtreeCreated' && change.target.name == 'New') {
        observerTwoCallCount++;
        // The second observer should get called twice and on the second time we
        // remove it and pass the test.
        chrome.test.assertTrue(observerTwoCallCount <= 2);
        if (observerTwoCallCount == 2) {
          // TODO(tjudkins): As mentioned above we remove the observer on a
          // timeout to ensure we are not modifying the list of observers while
          // it is still being iterated over. We should fix this and support
          // observers being able to remove themselves when triggered.
          setTimeout(() => {
            chrome.automation.removeTreeChangeObserver(observerTwo);
            // We call succeed on a short timeout to ensure all observers have
            // been called.
            setTimeout(chrome.test.succeed, 0);
          }, 0);
        }
      }
    };
    chrome.automation.addTreeChangeObserver('allTreeChanges', observerOne);
    chrome.automation.addTreeChangeObserver('allTreeChanges', observerTwo);

    // Trigger the button, which will add an element to the page and cause our
    // observers to receive tree change events.
    addButton.doDefault();
  },

  function testTreeChangedObserverForRemovingNode() {
    chrome.automation.addTreeChangeObserver("allTreeChanges", function(change) {
      if (change.type == "nodeRemoved" && change.target.role == "listItem") {
        chrome.test.succeed();
      }
    });

    var removeButton = rootNode.find({ attributes: { name: 'Remove' }});
    removeButton.doDefault();
  },

  function testTreeChangedObserverForLiveRegionsOnly() {
    // This test would fail if we set the filter to allTreeChanges.
    chrome.automation.addTreeChangeObserver(
        "liveRegionTreeChanges",
        function(change) {
      if (change.target.name == 'Dead') {
        // The internal bindings will notify us of a subtreeUpdateEnd if there
        // was a live region within the updates sent during unserialization. The
        // target in this case is picked by simply choosing the first target in
        // all tree changes, which could have been anything.
        if (change.type != 'subtreeUpdateEnd')
          chrome.test.fail();
      }
      // TODO(tjudkins): This test currently ends up calling chrome.test.succeed
      // 3 separate times for different tree changed events that fit the
      // condition it sets. We should probably also make it conditional on the
      // change.type and limit it to one of nodeChanged, nodeCreated or
      // textChanged.
      if (change.target.name == 'Live') {
        chrome.test.succeed();
      }
    });

    var liveButton = rootNode.find({ attributes: { name: 'Live' }});
    liveButton.doDefault();
  }
];

setUpAndRunTabsTests(allTests, 'tree_change.html');
