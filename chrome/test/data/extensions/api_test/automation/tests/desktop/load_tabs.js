// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var html = '<button>alpha</button><input type="text">hello</input>';

function getAllWebViews() {
  function findAllWebViews(node, nodes) {
    if (node.role == chrome.automation.RoleType.WEB_VIEW)
      nodes.push(node);

    var children = node.children;
    for (var i = 0; i < children.length; i++) {
      var child = findAllWebViews(children[i], nodes);
    }
  }

  var webViews = [];
  findAllWebViews(rootNode, webViews);
  return webViews;
}

var allTests = [
  function testLoadTabs() {
    runWithDocument(html, function() {
      var webViews = getAllWebViews();
      assertEq(1, webViews.length);
      var subroot = webViews[0].firstChild;
      assertEq(webViews[0], subroot.parent);
      assertEq(subroot, subroot.parent.children[0]);
      var button = subroot.firstChild.firstChild;
      assertEq(chrome.automation.RoleType.BUTTON, button.role);
      var input = subroot.firstChild.lastChild.previousSibling;
      assertEq(chrome.automation.RoleType.TEXT_FIELD, input.role);
      chrome.test.succeed();
    });
  },

  function testSubevents() {
    runWithDocument(html, function(subroot) {
      var button = null;

      rootNode.addEventListener(
          chrome.automation.EventType.FOCUS, function(evt) {
            if (button == evt.target) {
              chrome.test.succeed();
            }
          }, false);

      button = subroot.firstChild.firstChild;
      button.focus();
    });
  }
];

setUpAndRunDesktopTests(allTests);
