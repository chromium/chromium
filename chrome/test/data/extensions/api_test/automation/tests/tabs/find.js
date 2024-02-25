// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var group;
var h1;
var p1;
var link;
var main;
var p2;
var p3;
var img;
var okButton;
var cancelButton;

function initializeNodes(rootNode) {
  group = rootNode.firstChild;
  assertEq(RoleType.GROUP, group.role);

  h1 = group.firstChild;
  assertEq(RoleType.HEADING, h1.role);
  assertEq(1, h1.hierarchicalLevel);

  p1 = group.lastChild;
  assertEq(RoleType.PARAGRAPH, p1.role);

  link = p1.children[1];
  assertEq(RoleType.LINK, link.role);

  main = rootNode.children[1];
  assertEq(RoleType.MAIN, main.role);

  p2 = main.firstChild;
  assertEq(RoleType.PARAGRAPH, p2.role);

  strong = p2.lastChild;
  assertEq(RoleType.STRONG, strong.role);

  p3 = main.lastChild;
  assertEq(RoleType.PARAGRAPH, p3.role);

  img = rootNode.children[2];
  assertEq(RoleType.IMAGE, img.role);

  okButton = rootNode.children[3];
  assertEq(RoleType.BUTTON, okButton.role);
  assertEq('Ok', okButton.name);
  assertEq('disabled', okButton.restriction);

  cancelButton = rootNode.children[4];
  assertEq(RoleType.BUTTON, cancelButton.role);
  assertEq('Cancel', cancelButton.name);
  assertEq(undefined, cancelButton.restriction);
}

var allTests = [
  function testFindByRole() {
    initializeNodes(rootNode);

    // Should find the only instance of this role.
    assertEq(h1, rootNode.find({role: RoleType.HEADING}));
    assertEq([h1], rootNode.findAll({role: RoleType.HEADING}));

    // Should find the only instance of this role.
    assertEq(img, rootNode.find({role: RoleType.IMAGE}));
    assertEq([img], rootNode.findAll({role: RoleType.IMAGE}));

    // find should find first instance only.
    assertEq(okButton, rootNode.find({role: RoleType.BUTTON}));
    assertEq(p1, rootNode.find({role: RoleType.PARAGRAPH}));

    // findAll should find all instances.
    assertEq(
        [okButton, cancelButton], rootNode.findAll({role: RoleType.BUTTON}));
    assertEq([p1, p2, p3], rootNode.findAll({role: RoleType.PARAGRAPH}));

    // No instances: find should return null; findAll should return empty array.
    assertEq(null, rootNode.find({role: RoleType.CHECKBOX}));
    assertEq([], rootNode.findAll({role: RoleType.CHECKBOX}));

    // Calling from node should search only its subtree.
    assertEq(p1, group.find({role: RoleType.PARAGRAPH}));
    assertEq(p2, main.find({role: RoleType.PARAGRAPH}));
    assertEq([p2, p3], main.findAll({role: RoleType.PARAGRAPH}));

    chrome.test.succeed();
  },

  function testFindByStates() {
    initializeNodes(rootNode);

    // Find all focusable elements (disabled button is not focusable).
    assertEq(link, rootNode.find({state: {focusable: true}}));
    assertEq(
        [link, cancelButton], rootNode.findAll({state: {focusable: true}}));

    // Find disabled buttons.
    assertEq(
        okButton,
        rootNode.find({role: RoleType.BUTTON,
            attributes: {restriction: 'disabled'}}));
    assertEq(
        [okButton],
        rootNode.findAll({role: RoleType.BUTTON,
            attributes: {restriction: 'disabled'}}));

    // Find enabled buttons.
    assertEq(
        cancelButton,
        rootNode.find({role: RoleType.BUTTON,
            attributes: {restriction: undefined }}));
    assertEq(
        [cancelButton],
        rootNode.findAll({role: RoleType.BUTTON,
            attributes: {restriction: undefined }}));
    chrome.test.succeed();
  },

  function testFindByAttribute() {
    initializeNodes(rootNode);

    // Find by name attribute.
    assertEq(okButton, rootNode.find({attributes: {name: 'Ok'}}));
    assertEq(cancelButton, rootNode.find({attributes: {name: 'Cancel'}}));

    // String attributes must be exact match unless a regex is used.
    assertEq(null, rootNode.find({attributes: {name: 'Canc'}}));
    assertEq(null, rootNode.find({attributes: {name: 'ok'}}));

    // Find by value attribute - regexp.
    var query = {attributes: {name: /relationship/}};
    assertEq(strong, rootNode.find(query).parent);

    // Find by role and hierarchicalLevel attribute.
    assertEq(
        h1, rootNode.find(
                {role: RoleType.HEADING, attributes: {hierarchicalLevel: 1}}));
    assertEq(
        [], rootNode.findAll(
                {role: RoleType.HEADING, attributes: {hierarchicalLevel: 2}}));

    // Searching for an attribute which no element has fails.
    assertEq(null, rootNode.find({attributes: {charisma: 12}}));

    // Searching for an attribute value of the wrong type fails (even if the
    // value is equivalent).
    assertEq(
        null,
        rootNode.find(
            {role: RoleType.HEADING, attributes: {hierarchicalLevel: true}}));

    chrome.test.succeed();
  },

  function testMatches() {
    initializeNodes(rootNode);
    assertTrue(
        h1.matches({role: RoleType.HEADING}),
        'h1 should match RoleType.HEADING');
    assertTrue(
        h1.matches(
            {role: RoleType.HEADING, attributes: {hierarchicalLevel: 1}}),
        'h1 should match RoleType.HEADING and hierarchicalLevel: 1');
    assertFalse(
        h1.matches({
          role: RoleType.HEADING,
          state: {focusable: true},
          attributes: {hierarchicalLevel: 1}
        }),
        'h1 should not match focusable: true');
    assertTrue(
        h1.matches({
          role: RoleType.HEADING,
          state: {focusable: false},
          attributes: {hierarchicalLevel: 1}
        }),
        'h1 should match focusable: false');

    var p2StaticText = strong.firstChild;
    assertTrue(
        p2StaticText.matches(
            {role: RoleType.STATIC_TEXT, attributes: {name: /relationship/}}),
        'p2StaticText should match name: /relationship/ (regex match)');
    assertFalse(
        p2StaticText.matches(
            {role: RoleType.STATIC_TEXT, attributes: {name: 'relationship'}}),
        'p2 should not match name: \'relationship');

    chrome.test.succeed();
  }
];

setUpAndRunTabsTests(allTests, 'complex.html');
