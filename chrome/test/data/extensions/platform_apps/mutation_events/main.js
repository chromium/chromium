// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function noMutationEvents() {
    var failures = [];
    function addListener(node) {
      return function(type) {
        node.addEventListener(type, function() {
          failures.push('Received a ' + type + ' event');
        });
      };
    };
    var body = document.body;
    [ 'DOMSubtreeModified',
      'DOMNodeInserted',
      'DOMNodeRemoved',
      'DOMAttrModified',
      'DOMCharacterDataModified' ].forEach(addListener(body));

    var div = document.createElement('div');
    [ 'DOMNodeInsertedIntoDocument',
      'DOMNodeRemovedFromDocument' ].forEach(addListener(div));

    // DOMSubtreeModified, DOMNodeInserted{,IntoDocument}
    body.appendChild(div);
    // DOMSubtreeModified, DOMAttrModified
    div.setAttribute('data-foo', 'bar');
    // DOMNodeInserted
    var text = div.appendChild(document.createTextNode('hello'));
    // DOMCharacterDataModified
    text = 'goodbye';
    // DOMNodeRemoved, DOMNodeRemovedFromDocument
    body.removeChild(div);

    if (failures.length)
      chrome.test.fail(failures.join('\n'));
    else
      chrome.test.succeed();
  }
]);
