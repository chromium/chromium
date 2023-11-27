// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var RoleType = chrome.automation.RoleType;

var allTests = [
  function boundsForRange() {
    function getNthListItemInlineTextBox(index) {
      var list = rootNode.find({role: RoleType.LIST});
      var listItem = list.children[index];
      assertEq(RoleType.LIST_ITEM, listItem.role);
      var staticText = listItem.children[1];
      assertEq(RoleType.STATIC_TEXT, staticText.role);
      var inlineTextBox = staticText.firstChild;
      assertEq(RoleType.INLINE_TEXT_BOX, inlineTextBox.role);
      return inlineTextBox;
    }

    // Left-to-right.
    var ltr = getNthListItemInlineTextBox(0);
    ltr.boundsForRange(
        0, 4,
        (firstHalf) => {ltr.boundsForRange(4, ltr.name.length, (secondHalf) => {
          var bounds = ltr.location;
          assertEq(bounds.top, firstHalf.top);
          assertEq(bounds.left, firstHalf.left);
          assertEq(bounds.height, firstHalf.height);
          assertEq(bounds.top, secondHalf.top);
          assertEq(bounds.height, secondHalf.height);
          assertTrue(secondHalf.left > bounds.left);
          assertTrue(firstHalf.width < bounds.width);
          assertTrue(secondHalf.width < bounds.width);
          assertTrue(
              Math.abs(bounds.width - firstHalf.width - secondHalf.width) < 3);
        })});

    // Right-to-left.
    var rtl = getNthListItemInlineTextBox(1);
    bounds = rtl.location;
    rtl.boundsForRange(0, 4, (firstHalf) => {
      rtl.boundsForRange(4, rtl.name.length, (secondHalf) => {
        assertEq(bounds.top, secondHalf.top);
        assertTrue(Math.abs(bounds.left - secondHalf.left) < 3);
        assertEq(bounds.height, secondHalf.height);
        assertEq(bounds.top, firstHalf.top);
        assertEq(bounds.height, firstHalf.height);
        assertTrue(firstHalf.left > bounds.left);
        assertTrue(secondHalf.width < bounds.width);
        assertTrue(firstHalf.width < bounds.width);
        assertTrue(
            Math.abs(bounds.width - secondHalf.width - firstHalf.width) < 3);
      });
    });

    // Top-to-bottom.
    var ttb = getNthListItemInlineTextBox(2);
    var bounds = ttb.location;
    ttb.boundsForRange(0, 4, (firstHalf) => {
      ttb.boundsForRange(4, ttb.name.length, (secondHalf) => {
        assertEq(bounds.left, firstHalf.left);
        assertEq(bounds.top, firstHalf.top);
        assertEq(bounds.width, firstHalf.width);
        assertEq(bounds.left, secondHalf.left);
        assertEq(bounds.width, secondHalf.width);
        assertTrue(secondHalf.top > bounds.top);
        assertTrue(firstHalf.height < bounds.height);
        assertTrue(secondHalf.height < bounds.height);
        assertTrue(
            Math.abs(bounds.height - firstHalf.height - secondHalf.height) < 3);
      });
    });

    // Bottom-to-top.
    var btt = getNthListItemInlineTextBox(3);
    bounds = btt.location;
    btt.boundsForRange(0, 4, (firstHalf) => {
      btt.boundsForRange(4, btt.name.length, (secondHalf) => {
        assertEq(bounds.left, secondHalf.left);
        assertTrue(Math.abs(bounds.top - secondHalf.top) < 3);
        assertEq(bounds.width, secondHalf.width);
        assertEq(bounds.left, firstHalf.left);
        assertEq(bounds.width, firstHalf.width);
        assertTrue(firstHalf.top > bounds.top);
        assertTrue(secondHalf.height < bounds.height);
        assertTrue(firstHalf.height < bounds.height);
        assertTrue(
            Math.abs(bounds.height - secondHalf.height - firstHalf.height) < 3);
        chrome.test.succeed();
      });
    });
  },

  function boundsForRangeClips() {
    let clipped = rootNode.find(
        {role: 'inlineTextBox', attributes: {name: 'This text overflows'}});
    clipped.boundsForRange(0, clipped.name.length, (clippedBounds) => {
      assertTrue(
          clipped.parent.location.width < clipped.unclippedLocation.width);
      assertEq(clipped.parent.location.width, clippedBounds.width);
    });

    // The static text parent has 4 children, one for each word. The small box
    // size causes the words to be layed out on different lines, creating four
    // individual inlineTextBox nodes.
    let hiddenParent = rootNode.find(
        {role: 'staticText', attributes: {name: 'This text is hidden'}});
    assertEq(hiddenParent.children.length, 4);
    let hiddenChild = hiddenParent.children[0];
    hiddenChild.boundsForRange(0, hiddenChild.name.length, (hiddenBounds) => {
      assertTrue(
          hiddenParent.location.width < hiddenChild.unclippedLocation.width);
      assertTrue(
          hiddenParent.location.height < hiddenChild.unclippedLocation.height);
      assertEq(hiddenParent.location.width, hiddenBounds.width);
      assertEq(hiddenParent.location.height, hiddenBounds.height);
    });

    chrome.test.succeed();
  },

  function boundsForRangeUnclipped() {
    let overflowText = rootNode.find(
        {role: 'inlineTextBox', attributes: {name: 'This text overflows'}});
    overflowText.unclippedBoundsForRange(
        0, overflowText.name.length, (unclippedBounds) => {
          assertTrue(
              overflowText.parent.location.width <
              overflowText.unclippedLocation.width);
          // Since bounds may differ in different platforms, we set a 2 px
          // tolerance.
          assertTrue(
              Math.abs(
                  overflowText.unclippedLocation.width -
                  unclippedBounds.width) <= 2);
        });

    // The static text parent has 4 children, one for each word. The small box
    // size causes the words to be laid out on different lines, creating four
    // individual inlineTextBox nodes.
    let hiddenParent = rootNode.find(
        {role: 'staticText', attributes: {name: 'This text is hidden'}});
    assertEq(hiddenParent.children.length, 4);
    let hiddenChild = hiddenParent.children[0];
    hiddenChild.unclippedBoundsForRange(
        0, hiddenChild.name.length, (hiddenBounds) => {
          assertTrue(
              hiddenParent.location.width <
              hiddenChild.unclippedLocation.width);
          assertTrue(
              hiddenParent.location.height <
              hiddenChild.unclippedLocation.height);
          // Since bounds may differ in different platforms, we set a 2 px
          // tolerance.
          assertTrue(
              Math.abs(
                  hiddenChild.unclippedLocation.width - hiddenBounds.width) <=
              2);
          assertEq(hiddenChild.unclippedLocation.height, hiddenBounds.height);
        });

    chrome.test.succeed();
  }
];

setUpAndRunTabsTests(allTests, 'bounds_for_range.html');
