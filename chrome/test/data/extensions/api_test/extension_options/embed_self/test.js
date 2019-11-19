// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function optionsPageLoaded() {
  var hasLoaded = false;
  chrome.extension.getViews().forEach(function(view) {
    if (view.document.location.pathname == '/options.html') {
      chrome.test.assertEq(false, hasLoaded);
      hasLoaded = view.loaded;
    }
  });
  return hasLoaded;
}

function assertSenderIsOptionsPage(sender) {
  chrome.test.assertEq({
    'id': chrome.runtime.id,
    'url': chrome.runtime.getURL('options.html')
  }, sender);
}

chrome.test.runTests([
  // Basic tests that ensure that the <extensionoptions> guest view is created
  // and loaded, and that the load event is accurate.
  // createGuestViewDOM tests that it can be created and manipulated like a DOM
  // element, and createGuestViewProgrammatic tests the same but as a JavaScript
  // object.
  function createGuestViewDOM() {
    var extensionoptions = document.createElement('extensionoptions');
    extensionoptions.addEventListener('load', function() {
      try {
        chrome.test.assertTrue(optionsPageLoaded());
        chrome.test.succeed();
      } finally {
        document.body.removeChild(extensionoptions);
      }
    });
    extensionoptions.setAttribute('extension', chrome.runtime.id);
    document.body.appendChild(extensionoptions);
  },

  function createGuestViewProgrammatic() {
    var extensionoptions = new ExtensionOptions();
    extensionoptions.onload = function() {
      try {
        chrome.test.assertTrue(optionsPageLoaded());
        chrome.test.succeed();
      } finally {
        document.body.removeChild(extensionoptions);
      }
    };
    extensionoptions.extension = chrome.runtime.id;
    document.body.appendChild(extensionoptions);
  },

  // Tests if the <extensionoptions> guest view is successfully created and can
  // communicate with the embedder. This test expects that the guest options
  // page will add a {'pass': true} property to every Window and fire the
  // runtime.onMessage event with a short message.
  function canCommunicateWithGuest() {
    var done = chrome.test.listenForever(chrome.runtime.onMessage,
        function(message, sender, sendResponse) {
      assertSenderIsOptionsPage(sender);
      if (message == 'ready') {
        sendResponse('canCommunicateWithGuest');
      } else if (message == 'done') {
        try {
          var views = chrome.extension.getViews();
          chrome.test.assertEq(2, views.length);
          views.forEach(function(view) {
            chrome.test.assertTrue(view.pass);
          });
          chrome.test.assertEq(chrome.runtime.id, sender.id);
          done();
        } finally {
          document.body.removeChild(extensionoptions);
        }
      }
    });

    var extensionoptions = document.createElement('extensionoptions');
    extensionoptions.setAttribute('extension', chrome.runtime.id);
    document.body.appendChild(extensionoptions);
  },

  // Tests if the <extensionoptions> guest view can access the chrome.storage
  // API, a privileged extension API.
  function guestCanAccessStorage() {
    var onStorageChanged = false;
    var onSetAndGet = false;

    chrome.test.listenOnce(chrome.storage.onChanged, function(change) {
      chrome.test.assertEq(42, change.test.newValue);
    });

    // Listens for messages from the options page.
    var done = chrome.test.listenForever(chrome.runtime.onMessage,
        function(message, sender, sendResponse) {
      assertSenderIsOptionsPage(sender);

      // Options page is waiting for a command
      if (message == 'ready') {
        sendResponse('guestCanAccessStorage');
      // Messages from the options page containing test data
      } else if (message.description == 'onStorageChanged') {
        chrome.test.assertEq(message.expected, message.actual);
        onStorageChanged = true;
      } else if (message.description == 'onSetAndGet') {
        chrome.test.assertEq(message.expected, message.actual);
        onSetAndGet = true;
      }

      if (onStorageChanged && onSetAndGet) {
        document.body.removeChild(extensionoptions);
        done();
      }
    });

    var extensionoptions = document.createElement('extensionoptions');
    extensionoptions.setAttribute('extension', chrome.runtime.id);
    document.body.appendChild(extensionoptions);
  },

  function externalLinksOpenInNewTab() {
    var done = chrome.test.listenForever(chrome.runtime.onMessage,
        function(message, sender, sendResponse) {
      assertSenderIsOptionsPage(sender);

      if (message == 'ready') {
        sendResponse('externalLinksOpenInNewTab');
      } else if (message == 'done') {
        try {
          chrome.tabs.query({url: 'http://www.chromium.org/'}, function(tabs) {
            chrome.test.assertEq(1, tabs.length);
            chrome.test.assertEq('http://www.chromium.org/',
                                 tabs[0].url || tabs[0].pendingUrl);
            done();
          });
        } finally {
          document.body.removeChild(extensionoptions);
        }
      }
    });

    var extensionoptions = document.createElement('extensionoptions');
    extensionoptions.setAttribute('extension', chrome.runtime.id);
    document.body.appendChild(extensionoptions);
  }
]);
