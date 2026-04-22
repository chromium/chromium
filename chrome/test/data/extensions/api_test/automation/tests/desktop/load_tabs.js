// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const html = '<button>alpha</button><input type=' text '>hello</input>';

function getAllWebViews() {
  function findAllWebViews(node, nodes) {
    if (node.role == chrome.automation.RoleType.WEB_VIEW)
      nodes.push(node);

    const children = node.children;
    for (let i = 0; i < children.length; i++) {
      const child = findAllWebViews(children[i], nodes);
    }
  }

  let webViews = [];
  findAllWebViews(rootNode, webViews);
  return webViews;
}

const allTests = [
  function testLoadTabs() {
    runWithDocument(html, function() {
      let webViews = getAllWebViews();
      assertEq(1, webViews.length);
      const subroot = webViews[0].firstChild;
      assertEq(webViews[0], subroot.parent);
      assertEq(subroot, subroot.parent.children[0]);
      let button = subroot.firstChild.firstChild;
      assertEq(chrome.automation.RoleType.BUTTON, button.role);
      const input = subroot.firstChild.lastChild.previousSibling;
      assertEq(chrome.automation.RoleType.TEXT_FIELD, input.role);
      chrome.test.succeed();
    });
  },

  function testSubevents() {
    runWithDocument(html, function(subroot) {
      let button = null;

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
