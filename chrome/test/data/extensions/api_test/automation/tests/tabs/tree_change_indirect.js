// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test to ensure that labels and descriptions that come from elsewhere in the
// tree are updated when the related content changes.

// TODO(aleventhal) why isn't this working?
// function findById(id) {
// TODO(accessibility): Verify that the following line really works.
// return rootNode.find({ htmlId: id });

var allTests = [
  function testUpdateRelatedNamesAndDescriptions() {
    var cats = rootNode.find({ role: "checkBox"});
    var apples = rootNode.find({ role: "main" } );
    var butter = rootNode.find({ role: "group" } );
    // TODO(aleventhal) why are we getting the wrong objects for these?
    // var cats = findById("input");
    // var apples = findById("main");
    // var butter = findById("group");
    assertEq("cats", cats.name);
    assertEq("apples", apples.name);
    assertEq("butter", butter.description);

    chrome.automation.addTreeChangeObserver("allTreeChanges", function(change) {
      if (change.type == "textChanged") {
        // TODO(aleventhal) Why is timeout necessary to avoid uncaught exception
        // "Error in event handler for automationInternal.onTreeChange:
        // ReferenceError: treeChange is not defined" ?
        setTimeout(function() {
          var dogs = rootNode.find({ role: "checkBox"});
          var oranges = rootNode.find({ role: "main" } );
          var margarine = rootNode.find({ role: "group"} );
          assertEq("dogs", dogs.name);
          assertEq("oranges", oranges.name);
          assertEq("margarine", margarine.description);
          chrome.test.succeed();
        }, 0);
      }
    });

    var button = rootNode.find({ attributes: { name: "Change" }});
    button.doDefault();
  },
];

setUpAndRunTabsTests(allTests, "tree_change_indirect.html");
