// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var fail = chrome.test.fail;
var succeed = chrome.test.succeed;

function assertThrowsError(method, opt_expectedError) {
  try {
    method();
  } catch (e) {
    var message = e.message || e;
    if (opt_expectedError) {
      assertEq(opt_expectedError, e.name);
    } else {
      assertTrue(
          message.indexOf('is not available in packaged apps') != -1,
          'Unexpected message ' + message);
    }
    return;
  }

  fail('error not thrown');
}

chrome.test.runTests([
  function testDocumentBenignMethods() {
    // The real document.open returns a document.
    assertEq('undefined', typeof(document.open()));

    // document.clear() has been deprecated on the Web as well, so there is no
    // good method of testing that the method has been stubbed. We have to
    // settle for testing that calling the method doesn't throw.
    assertEq('undefined', typeof(document.clear()));

    // document.close() doesn't do anything on its own, so there is good method
    // of testing that it has been stubbed. Settle for making sure it doesn't
    // throw.
    assertEq('undefined', typeof(document.close()));

    succeed();
  },

  function testDocumentEvilMethods() {
    assertThrowsError(document.write);
    assertThrowsError(document.writeln);

    succeed();
  },

  function testDocumentGetters() {
    assertEq('undefined', typeof(document.all));
    assertEq('undefined', typeof(document.bgColor));
    assertEq('undefined', typeof(document.fgColor));
    assertEq('undefined', typeof(document.alinkColor));
    assertEq('undefined', typeof(document.linkColor));
    assertEq('undefined', typeof(document.vlinkColor));

    succeed();
  },

  function testHistory() {
    // Accessing these logs warnings to the console.
    assertEq('undefined', typeof(history.back));
    assertEq('undefined', typeof(history.forward));
    assertEq('undefined', typeof(history.go));
    assertEq('undefined', typeof(history.length));
    assertEq('undefined', typeof(history.pushState));
    assertEq('undefined', typeof(history.replaceState));
    assertEq('undefined', typeof(history.state));
    succeed();
  },

  function testWindowFind() {
    assertEq('undefined', typeof(Window.prototype.find));
    assertEq('undefined', typeof(window.find('needle')));
    assertEq('undefined', typeof(find('needle')));
    succeed();
  },

  function testWindowAlert() {
    assertEq('undefined', typeof(Window.prototype.alert));
    assertEq('undefined', typeof(window.alert()));
    assertEq('undefined', typeof(alert()));
    succeed();
  },

  function testWindowConfirm() {
    assertEq('undefined', typeof(Window.prototype.confirm));
    assertEq('undefined', typeof(window.confirm('Failed')));
    assertEq('undefined', typeof(confirm('Failed')));
    succeed();
  },

  function testWindowPrompt() {
    assertEq('undefined', typeof(Window.prototype.prompt));
    assertEq('undefined', typeof(window.prompt('Failed')));
    assertEq('undefined', typeof(prompt('Failed')));
    succeed();
  },

  function testBars() {
    var bars = ['locationbar', 'menubar', 'personalbar',
                'scrollbars', 'statusbar', 'toolbar'];
    for (var x = 0; x < bars.length; x++) {
      assertEq('undefined', typeof(this[bars[x]]));
      assertEq('undefined', typeof(window[bars[x]]));
    }
    succeed();
  },

  function testBlockedEvents() {
    // Fails the test if called by dispatchEvent().
    var eventHandler = function() { fail('blocked event handled'); };

    var blockedEvents = ['unload', 'beforeunload'];

    for (var i = 0; i < blockedEvents.length; ++i) {
      window['on' + blockedEvents[i]] = eventHandler;
      assertEq(undefined, window['on' + blockedEvents[i]]);

      var event = new Event(blockedEvents[i]);
      window.addEventListener(blockedEvents[i], eventHandler);
      // Ensures that addEventListener did not actually register the handler.
      // If eventHandler is registered as a listener, it will be called by
      // dispatchEvent() and the test will fail.
      window.dispatchEvent(event);
      Window.prototype.addEventListener.apply(window,
          [blockedEvents[i], eventHandler]);
      window.dispatchEvent(event);
    }

    succeed();
  },

  function testSyncXhr() {
    var xhr = new XMLHttpRequest();
    assertThrowsError(function() {
      xhr.open('GET', 'data:should not load', false);
    }, 'InvalidAccessError');
    succeed();
  },

  /**
   * Tests that restrictions apply to iframes as well.
   */
  function testIframe() {
    var iframe = document.createElement('iframe');
    iframe.onload = function() {
      assertThrowsError(iframe.contentWindow.document.write);
      succeed();
    };
    iframe.src = 'iframe.html';
    document.body.appendChild(iframe);
  },

  /**
   * Tests that restrictions apply to sandboxed iframes.
   */
  function testSandboxedIframe() {
    function handleWindowMessage(e) {
      if (e.data.success)
        succeed();
      else
        fail(e.data.reason);
    };
    window.addEventListener('message', handleWindowMessage);

    var iframe = document.createElement('iframe');
    iframe.src = 'sandboxed_iframe.html';
    document.body.appendChild(iframe);
  },

  function testLegacyApis() {
    if (chrome.app) {
      assertEq('undefined', typeof(chrome.app.getIsInstalled));
      assertEq('undefined', typeof(chrome.app.isInstalled));
      assertEq('undefined', typeof(chrome.app.getDetails));
      assertEq('undefined', typeof(chrome.app.runningState));
    }
    assertEq('undefined', typeof(chrome.extension));
    succeed();
  },

  function testExtensionApis() {
    assertEq('undefined', typeof(chrome.tabs));
    assertEq('undefined', typeof(chrome.windows));
    succeed();
  }
]);
