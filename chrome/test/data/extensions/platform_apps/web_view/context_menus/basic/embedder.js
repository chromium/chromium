// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(msg) { window.console.log(msg); };

function ContextMenuTester() {
  this.webview_ = null;
  this.id_ = '';

  this.inlineClickCalled_ = false;
  this.globalClickCalled_ = false;

  // Used for createThreeMenuItems().
  this.numItemsCreated_ = 0;

  this.failed_ = false;
}

ContextMenuTester.prototype.setWebview = function(webview) {
  this.webview_ = webview;
};

ContextMenuTester.prototype.testProperties = function() {
  LOG('testProperties');
  if (!this.webview_) {
    this.fail_();
    return;
  }

  var w = this.webview_;
  this.assertTrue_(!!w.contextMenus);
  this.assertEq_('function', typeof w.contextMenus.create);
  this.assertEq_('function', typeof w.contextMenus.update);
  this.assertEq_('function', typeof w.contextMenus.remove);
  this.assertEq_('function', typeof w.contextMenus.removeAll);

  var onClicked = w.contextMenus.onClicked;
  this.assertTrue_(!!onClicked);
  this.assertEq_('function', typeof onClicked.addListener);
  this.assertEq_('function', typeof onClicked.hasListener);
  this.assertEq_('function', typeof onClicked.hasListeners);
  this.assertEq_('function', typeof onClicked.removeListener);

  if (!this.failed_) {
    this.proceedTest_('ITEM_CHECKED');
  }
};

ContextMenuTester.prototype.testCreateMenuItem = function() {
  LOG('testCreateMenuItem');
  if (!this.webview_) {
    this.fail_();
    return;
  }

  var self = this;
  this.id_ = this.webview_.contextMenus.create({
    'title': 'initial-title',
    'onclick': function() { self.onClick_('inline'); }
  }, function createdCallback() {
    LOG('ITEM_CREATED');
    self.proceedTest_('ITEM_CREATED');
  });
  this.webview_.contextMenus.onClicked.addListener(function() {
    self.onClick_('global');
  });
};

ContextMenuTester.prototype.testUpdateMenuItem = function() {
  LOG('testUpdateMenuItem');
  if (!this.id_) {
    this.fail_();
    return;
  }

  var self = this;
  this.webview_.contextMenus.update(this.id_,
    {'title': 'new_title'},
    function() { self.proceedTest_('ITEM_UPDATED'); });
};

ContextMenuTester.prototype.testRemoveItem = function() {
  LOG('testRemoveItem');
  var self = this;
  this.webview_.contextMenus.remove(
      this.id_,
      function() { self.proceedTest_('ITEM_REMOVED'); });
};

ContextMenuTester.prototype.createThreeMenuItems = function() {
  LOG('createThreeMenuItems');
  var self = this;
  var createdCallback = function() { self.onCreate_(); };
  this.webview_.contextMenus.create({'title': 'a'}, createdCallback);
  this.webview_.contextMenus.create({'title': 'b'}, createdCallback);
  this.webview_.contextMenus.create({'title': 'c'}, createdCallback);
};

ContextMenuTester.prototype.testRemoveAllItems = function() {
  LOG('testRemoveAllItems');
  var self = this;
  this.webview_.contextMenus.removeAll(function() {
    self.proceedTest_('ITEM_ALL_REMOVED');
  });
};

ContextMenuTester.prototype.registerPreventDefault = function() {
  this.preventDefaultLister_ = function(e) {
    e.preventDefault();
    this.proceedTest_('DEFAULT_PREVENTED');
  }.bind(this);

  this.webview_.contextMenus.onShow.addListener(this.preventDefaultLister_);
};

ContextMenuTester.prototype.removePreventDefault = function() {
  if (!this.preventDefaultLister_) {
    LOG('Error: A listener is expected to setup prior to calling ' +
        'contextMenus.onShow');
    return;
  }
  this.webview_.contextMenus.onShow.removeListener(this.preventDefaultLister_);

  this.proceedTest_('PREVENT_DEFAULT_LISTENER_REMOVED');
};

ContextMenuTester.prototype.onClick_ = function(type) {
  if (type == 'global') {
    this.globalClickCalled_ = true;
  } else if (type == 'inline') {
    this.inlineClickCalled_ = true;
  }
  if (this.inlineClickCalled_ && this.globalClickCalled_) {
    this.proceedTest_('ITEM_CLICKED');
  }
};

ContextMenuTester.prototype.onCreate_ = function() {
  ++this.numItemsCreated_;
  if (this.numItemsCreated_ == 3) {
    this.proceedTest_('ITEM_MULTIPLE_CREATED');
  }
};

ContextMenuTester.prototype.proceedTest_ = function(step) {
  switch (step) {
    case 'ITEM_CHECKED':
      document.title = 'ITEM_CHECKED';
      break;
    case 'ITEM_CREATED':
      document.title = 'ITEM_CREATED';
      break;
    case 'ITEM_CLICKED':
      chrome.test.sendMessage('ITEM_CLICKED');
      break;
    case 'ITEM_UPDATED':
      document.title = 'ITEM_UPDATED';
      break;
    case 'ITEM_REMOVED':
      document.title = 'ITEM_REMOVED';
      break;
    case 'ITEM_MULTIPLE_CREATED':
      document.title = 'ITEM_MULTIPLE_CREATED';
      break;
    case 'ITEM_ALL_REMOVED':
      document.title = 'ITEM_ALL_REMOVED';
      break;
    case 'DEFAULT_PREVENTED':
      chrome.test.sendMessage('WebViewTest.CONTEXT_MENU_DEFAULT_PREVENTED');
      break;
    case 'PREVENT_DEFAULT_LISTENER_REMOVED':
      document.title = 'PREVENT_DEFAULT_LISTENER_REMOVED';
      break;
    default:
      break;
  }
};

ContextMenuTester.prototype.fail_ = function() {
  this.failed_ = true;
  document.title = 'error';
};

ContextMenuTester.prototype.assertEq_ = function(e, a) {
  if (e != a) {
    this.fail_();
  }
};

ContextMenuTester.prototype.assertTrue_ = function(c) {
  if (!c) {
    this.fail_();
  }
};

var tester = new ContextMenuTester();

// window.* exported functions begin.
window.checkProperties = function() {
  tester.testProperties();
};
window.createMenuItem = function() {
  tester.testCreateMenuItem();
};
window.updateMenuItem = function() {
  tester.testUpdateMenuItem();
};
window.removeItem = function() {
  tester.testRemoveItem();
};
window.createThreeMenuItems = function() {
  tester.createThreeMenuItems();
};
window.removeAllItems = function() {
  tester.testRemoveAllItems();
};
window.registerPreventDefault = function() {
  tester.registerPreventDefault();
};
window.removePreventDefault = function() {
  tester.removePreventDefault();
};
// window.* exported functions end.

function setUpTest(messageCallback) {
  var guestUrl = 'data:text/html,<html><body>guest</body></html>';
  var webview = document.createElement('webview');

  var onLoadStop = function(e) {
    LOG('webview has loaded.');
    webview.executeScript(
      {file: 'guest.js'},
      function(results) {
        if (!results || !results.length) {
          chrome.test.sendMessage('WebViewTest.FAILURE');
          return;
        }
        LOG('Script has been injected into webview.');
        // Establish a communication channel with the guest.
        var msg = ['connect'];
        webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      });
  };
  webview.addEventListener('loadstop', onLoadStop);

  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == 'connected') {
      console.log('A communication channel has been established with webview.');
    }
    messageCallback(webview);
  });

  webview.setAttribute('src', guestUrl);
  document.body.appendChild(webview);
}

onload = function() {
  chrome.test.getConfig(function(config) {
    setUpTest(function(webview) {
      LOG('Guest load completed.');
      chrome.test.sendMessage('WebViewTest.LAUNCHED');
      tester.setWebview(webview);
    });
  });
};
