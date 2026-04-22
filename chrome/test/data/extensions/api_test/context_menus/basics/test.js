// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const assertNoLastError = chrome.test.assertNoLastError;
const inServiceWorker = 'ServiceWorkerGlobalScope' in self;
let currentId = 1;

// Add a unique menu ID if this is running in a Service Worker-based
// extension and the 'id' property doesn't exist.
function maybeAddId(createProperties) {
  if (inServiceWorker && typeof createProperties['id'] === 'undefined') {
    createProperties['id'] = String(currentId++);
  }
  return createProperties;
}

const tests = [
  function simple() {
    chrome.contextMenus.create(
        maybeAddId({title: '1'}), chrome.test.callbackPass());
  },

  function no_properties() {
    chrome.contextMenus.create({}, function(id) {
      chrome.test.assertNe(null, chrome.runtime.lastError);
      chrome.test.succeed();
    });
  },

  function remove() {
    let id;
    id = chrome.contextMenus.create(maybeAddId({title: '1'}), function() {
      assertNoLastError();
      chrome.contextMenus.remove(id, chrome.test.callbackPass());
    });
  },

  function update() {
    let id;
    id = chrome.contextMenus.create(
        maybeAddId({title: 'update test'}), function() {
          assertNoLastError();
          chrome.contextMenus.update(
              id, {title: 'test2'}, chrome.test.callbackPass());
        });

    chrome.contextMenus.create(
        {id: 'test3', type: 'checkbox', title: 'test3'}, function() {
          assertNoLastError();
          // Calling update without specifying "type" should not change the menu
          // item's type to "normal" and therefore setting "checked" should not
          // fail.
          chrome.contextMenus.update(
              'test3', {checked: true}, chrome.test.callbackPass());
        });
  },

  function removeAll() {
    chrome.contextMenus.create(maybeAddId({title: '1'}), function() {
      assertNoLastError();
      chrome.contextMenus.create(maybeAddId({title: '2'}), function() {
        assertNoLastError();
        chrome.contextMenus.removeAll(chrome.test.callbackPass());
      });
    });
  },

  function hasParent() {
    let id;
    id = chrome.contextMenus.create(maybeAddId({title: 'parent'}), function() {
      assertNoLastError();
      chrome.contextMenus.create(
          maybeAddId({title: 'child', parentId: id}), function() {
            assertNoLastError();
            chrome.test.succeed();
          });
    });
  },
];


// Add tests for creating menu item with various types and contexts.
const types = ['checkbox', 'radio', 'separator'];
const contexts =
    ['all', 'page', 'selection', 'link', 'editable', 'image', 'video', 'audio'];
function makeCreateTest(type, contexts) {
  const result = function() {
    let title = type;
    if (contexts && contexts.length > 0) {
      title += ` ${contexts.join(',')}`;
    }
    const properties = maybeAddId({title: title, type: type});

    chrome.contextMenus.create(properties, chrome.test.callbackPass());
  };
  result.generatedName =
      `create_${type}` + (contexts ? `-${contexts.join(',')}` : '');
  return result;
}

for (const i in types) {
  tests.push(makeCreateTest(types[i]));
}
for (const i in contexts) {
  tests.push(makeCreateTest('normal', [contexts[i]]));
}

// Add tests for various errors when using promise based calls. Note: since
// promise based API signatures are only exposed in MV3, we only add these for
// the service worker tests.
if (inServiceWorker) {
  tests.push(
      async function updateWithUnusedId() {
        await chrome.test.assertPromiseRejects(
            chrome.contextMenus.update('unused-id', {title: 'Non-existing'}),
            'Error: Cannot find menu item with id unused-id');
        chrome.test.succeed();
      },

      async function removeWithUnusedId() {
        await chrome.test.assertPromiseRejects(
            chrome.contextMenus.remove('unused-id'),
            'Error: Cannot find menu item with id unused-id');
        chrome.test.succeed();
      },
  );
}

chrome.test.runTests(tests);
