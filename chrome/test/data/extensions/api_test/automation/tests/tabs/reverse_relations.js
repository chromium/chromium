// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const allTests = [
  function testDetailsReverseRelations() {
    const detailsFrom = rootNode.find({attributes: {name: 'DetailsFrom'}});
    const detailsTo = rootNode.find({attributes: {name: 'DetailsTo'}});
    assertEq(detailsFrom.details[0], detailsTo);
    assertEq(detailsTo.detailsFor.length, 1);
    assertEq(detailsTo.detailsFor[0], detailsFrom);
    chrome.test.succeed();
  },

  function testLabelledByReverseRelations() {
    const input = rootNode.find({role: RoleType.TEXT_FIELD});
    const label1 = rootNode.find({attributes: {name: 'Label1'}});
    const label2 = rootNode.find({attributes: {name: 'Label2'}});
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

setUpAndRunTabsTests(allTests, 'reverse_relations.html');
