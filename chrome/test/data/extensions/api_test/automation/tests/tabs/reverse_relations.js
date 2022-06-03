// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testDetailsReverseRelations() {
    var detailsFrom = rootNode.find({attributes: {name: 'DetailsFrom'}});
    var detailsTo = rootNode.find({attributes: {name: 'DetailsTo'}});
    assertEq(detailsFrom.details[0], detailsTo);
    assertEq(detailsTo.detailsFor.length, 1);
    assertEq(detailsTo.detailsFor[0], detailsFrom);
    chrome.test.succeed();
  },

  function testLabelledByReverseRelations() {
    var input = rootNode.find({role: RoleType.TEXT_FIELD});
    var label1 = rootNode.find({attributes: {name: 'Label1'}});
    var label2 = rootNode.find({attributes: {name: 'Label2'}});
    assertEq(input.labelledBy.length, 2);
    assertEq(input.labelledBy[0], label1);
    assertEq(input.labelledBy[1], label2);
    assertEq(label1.labelFor.length, 1);
    assertEq(label1.labelFor[0], input);
    assertEq(label2.labelFor.length, 1);
    assertEq(label2.labelFor[0], input);
    chrome.test.succeed();
  },
];

setUpAndRunTests(allTests, 'reverse_relations.html');
