// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.test = {};
embedder.baseGuestURL = '';
embedder.guestURL = '';
embedder.guestWithLinkURL = '';
embedder.newWindowURL = '';

window.runTest = function(testName) {
  if (!embedder.test.testList[testName]) {
    console.log('Incorrect testName: ' + testName);
    embedder.test.fail();
    return;
  }

  // Run the test.
  embedder.test.testList[testName]();
};
// window.* exported functions end.

/** @private */
embedder.setUp_ = function(config) {
  embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
  embedder.guestURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/newwindow' +
      '/guest_opener.html';
  embedder.newWindowURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/newwindow' +
      '/newwindow.html';
  chrome.test.log('Guest url is: ' + embedder.guestURL);
  embedder.guestWithLinkURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/newwindow' +
      '/guest_with_link.html';
  embedder.guestOpenOnLoadURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/newwindow' +
      '/guest_opener_open_on_load.html';
};

/** @private */
embedder.setUpGuest_ = function(partitionName) {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"></webview>';
  var webview = document.querySelector('webview');
  if (partitionName) {
    webview.partition = partitionName;
  }
  if (!webview) {
    embedder.test.fail('No <webview> element created');
  }
  return webview;
};

/** @private */
embedder.setUpNewWindowRequest_ = function(webview, url, frameName, testName) {
  var onWebViewLoadStop = function(e) {
    // Send post message to <webview> when it's ready to receive them.
    var redirect = testName.indexOf("_blank") != -1;
    webview.contentWindow.postMessage(
        JSON.stringify(
            ['open-window', '' + url, '' + frameName, redirect]), '*');
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
  webview.setAttribute('src', embedder.guestURL);
};

embedder.test = {};
embedder.test.succeed = function() {
  chrome.test.sendMessage('TEST_PASSED');
};

embedder.test.fail = function() {
  chrome.test.sendMessage('TEST_FAILED');
};

embedder.test.assertEq = function(a, b) {
  if (a != b) {
    console.log('assertion failed: ' + a + ' != ' + b);
    embedder.test.fail();
  }
};

embedder.test.assertTrue = function(condition) {
  if (!condition) {
    console.log('assertion failed: true != ' + condition);
    embedder.test.fail();
  }
};

embedder.test.assertFalse = function(condition) {
  if (condition) {
    console.log('assertion failed: false != ' + condition);
    embedder.test.fail();
  }
};

/** @private */
embedder.requestFrameName_ =
    function(webview, openerWebview, testName, expectedFrameName) {
  var onWebViewLoadStop = function(e) {
    // Send post message to <webview> when it's ready to receive them.
    // Note that loadstop will get called twice if the test is opening
    // a new window via a redirect: one for navigating to about:blank
    // and another for navigating to the final destination.
    // about:blank will not respond to the postMessage so it's OK
    // to send it again on the second loadstop event.
    webview.contentWindow.postMessage(
        JSON.stringify(['get-frame-name', testName]), '*');
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
  var onPostMessageReceived = function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == 'get-frame-name') {
      var name = data[1];
      if (testName != name)
        return;
      var frameName = data[2];
      embedder.test.assertEq(expectedFrameName, frameName);
      embedder.test.assertEq(expectedFrameName, webview.name);
      embedder.test.assertEq(openerWebview.partition, webview.partition);
      window.removeEventListener('message', onPostMessageReceived);
      embedder.test.succeed();
    }
  };
  window.addEventListener('message', onPostMessageReceived);
};

/** @private */
embedder.requestClose_ = function(webview, testName) {
  var onWebViewLoadStop = function(e) {
    webview.contentWindow.postMessage(
        JSON.stringify(['close', testName]), '*');
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
  var onWebViewClose = function(e) {
    webview.removeEventListener('close', onWebViewClose);
    embedder.test.succeed();
  };
  webview.addEventListener('close', onWebViewClose);
};

/** @private */
embedder.requestExecuteScript_ =
    function(webview, script, expectedResult, testName) {
  var onWebViewLoadStop = function(e) {
    webview.executeScript(
      {code: script},
      function(results) {
        embedder.test.assertEq(1, results.length);
        embedder.test.assertEq(expectedResult, results[0]);
        embedder.test.succeed();
      });
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
};

/** @private */
embedder.assertCorrectEvent_ = function(e, guestName) {
  embedder.test.assertEq('newwindow', e.type);
  embedder.test.assertTrue(!!e.targetUrl);
  embedder.test.assertEq(guestName, e.name);
};

// Tests begin.

var testNewWindowName = function(testName,
                                 webViewName,
                                 guestName,
                                 partitionName,
                                 expectedFrameName) {
  var webview = embedder.setUpGuest_(partitionName);

  var onNewWindow = function(e) {
    chrome.test.log('Embedder notified on newwindow');
    embedder.assertCorrectEvent_(e, guestName);

    var newwebview = document.createElement('webview');
    newwebview.setAttribute('name', webViewName);
    document.querySelector('#webview-tag-container').appendChild(newwebview);
    embedder.requestFrameName_(
        newwebview, webview, testName, expectedFrameName);
    try {
      e.window.attach(newwebview);
    } catch (e) {
      embedder.test.fail();
    }
  };
  webview.addEventListener('newwindow', onNewWindow);

  // Load a new window with the given name.
  embedder.setUpNewWindowRequest_(webview, 'guest.html', guestName, testName);
};

// Loads a guest which requests a new window.
function testNewWindowNameTakesPrecedence() {
  var webViewName = 'foo';
  var guestName = 'bar';
  var partitionName = 'foobar';
  var expectedName = guestName;
  testNewWindowName('testNewWindowNameTakesPrecedence',
                    webViewName, guestName, partitionName, expectedName);
}

function testNewWindowWebViewNameTakesPrecedence() {
  var webViewName = 'foo';
  var guestName = '';
  var partitionName = 'persist:foobar';
  var expectedName = webViewName;
  testNewWindowName('testNewWindowWebViewNameTakesPrecedence',
                    webViewName, guestName, partitionName, expectedName);
}

function testNewWindowNoName() {
  var webViewName = '';
  var guestName = '';
  var partitionName = '';
  var expectedName = '';
  testNewWindowName('testNewWindowNoName',
                    webViewName, guestName, partitionName, expectedName);
}

// This test exercises the need for queuing events that occur prior to
// attachment. In this test a new window is opened that initially navigates to
// about:blank and then subsequently redirects to its final destination. This
// test responds to loadstop in the new <webview>. Since "about:blank" does not
// have any external resources, it loads immediately prior to attachment, and
// the <webview> that is eventually attached never gets a chance to see the
// event. GuestView solves this problem by queuing events that occur prior to
// attachment and firing them immediately after attachment.
function testNewWindowRedirect() {
  var webViewName = 'foo';
  var guestName = '';
  var partitionName = 'persist:foobar';
  var expectedName = webViewName;
  testNewWindowName('testNewWindowRedirect_blank',
                    webViewName, guestName, partitionName, expectedName);
}

// Tests that we fail gracefully if we try to attach() a <webview> on a
// newwindow event after the opener has been destroyed.
function testNewWindowAttachAfterOpenerDestroyed() {
  var testName = 'testNewWindowAttachAfterOpenerDestroyed';
  var webview = embedder.setUpGuest_('foobar');

  var onNewWindow = function(e) {
    embedder.assertCorrectEvent_(e, '');

    // Remove the opener.
    webview.parentNode.removeChild(webview);
    // Pass in a timeout so we ensure the newwindow disposal codepath
    // works properly.
    window.setTimeout(function() {
      // At this point the opener <webview> is gone.
      // Trying to discard() will fail silently.
      e.window.attach(document.createElement('webview'));
      window.setTimeout(function() { embedder.test.succeed(); }, 0);
    }, 0);

    e.preventDefault();
  };
  webview.addEventListener('newwindow', onNewWindow);

  // Load a new window with the given name.
  embedder.setUpNewWindowRequest_(webview, 'guest.html', '', testName);
}

// Test that an embedder can attach a new window to a <webview> in a different
// frame.
async function testNewWindowAttachInSubFrame() {
  let testName = 'testNewWindowAttachInSubFrame';
  let webview = embedder.setUpGuest_('foobar');

  let subframe = document.createElement('iframe');
  subframe.src = '/subframe_with_webview.html';
  await new Promise((resolve) => {
    subframe.onload = resolve;
    document.body.appendChild(subframe);
  });

  webview.addEventListener('newwindow', (e) => {
    embedder.assertCorrectEvent_(e, '');

    let newwebview = subframe.contentDocument.querySelector('webview');
    newwebview.addEventListener('loadstop', embedder.test.succeed);
    try {
      e.window.attach(newwebview);
    } catch (e) {
      embedder.test.fail();
    }
  });

  embedder.setUpNewWindowRequest_(webview, 'guest.html', '', testName);
}

function testNewWindowClose() {
  var testName = 'testNewWindowClose';
  var webview = embedder.setUpGuest_('foobar');

  var onNewWindow = function(e) {
    chrome.test.log('Embedder notified on newwindow');
    embedder.assertCorrectEvent_(e, '');

    var newwebview = document.createElement('webview');
    document.querySelector('#webview-tag-container').appendChild(newwebview);
    embedder.requestClose_(newwebview, testName);
    try {
      e.window.attach(newwebview);
    } catch (e) {
      embedder.test.fail();
    }
  };
  webview.addEventListener('newwindow', onNewWindow);

  // Load a new window with the given name.
  embedder.setUpNewWindowRequest_(webview, 'guest.html', '', testName);
}

// Checks that calling event.window.attach() with a <webview> that is not
// in the DOM works properly.
// This tests the deferred attachment code path in web_view.js.
function testNewWindowDeferredAttachment() {
  var testName = 'testNewWindowDeferredAttachment';
  var webview = embedder.setUpGuest_('foobar');

  var onNewWindow = function(e) {
    chrome.test.log('Embedder notified on newwindow');
    embedder.assertCorrectEvent_(e, '');

    var newwebview = document.createElement('webview');

    newwebview.addEventListener('loadstop', function() {
      chrome.test.log('Guest in newwindow got loadstop.');
      embedder.test.succeed();
    });

    try {
      e.window.attach(newwebview);
    } catch (e) {
      embedder.test.fail();
    }

    // Append the <webview> in DOM later.
    window.setTimeout(function() {
      document.querySelector('#webview-tag-container').appendChild(newwebview);
    }, 0);
  };
  webview.addEventListener('newwindow', onNewWindow);

  // Load a new window with the given name.
  embedder.setUpNewWindowRequest_(webview, 'guest.html', '', testName);
}

// Tests that we fail gracefully if we try to discard() a <webview> on a
// newwindow event after the opener has been destroyed.
function testNewWindowDiscardAfterOpenerDestroyed() {
  var testName = 'testNewWindowDiscardAfterOpenerDestroyed';
  var webview = embedder.setUpGuest_('foobar');

  var onNewWindow = function(e) {
    embedder.assertCorrectEvent_(e, '');

    // Remove the opener.
    webview.parentNode.removeChild(webview);
    // Pass in a timeout so we ensure the newwindow disposal codepath
    // works properly.
    window.setTimeout(function() {
      // At this point the opener <webview> is gone.
      // Trying to discard() will fail silently.
      e.window.discard();
      window.setTimeout(function() { embedder.test.succeed(); }, 0);
    }, 0);

    e.preventDefault();
  };
  webview.addEventListener('newwindow', onNewWindow);

  // Load a new window with the given name.
  embedder.setUpNewWindowRequest_(webview, 'guest.html', '', testName);
}

function testNewWindowExecuteScript() {
  var testName = 'testNewWindowExecuteScript';
  var webview = embedder.setUpGuest_('foobar');

  var onNewWindow = function(e) {
    chrome.test.log('Embedder notified on newwindow');
    embedder.assertCorrectEvent_(e, '');

    var newwebview = document.createElement('webview');
    document.querySelector('#webview-tag-container').appendChild(newwebview);
    embedder.requestExecuteScript_(
        newwebview,
        'document.body.style.backgroundColor = "red";',
        'red',
        testName);
    try {
      e.window.attach(newwebview);
    } catch (e) {
      embedder.test.fail();
    }
  };
  webview.addEventListener('newwindow', onNewWindow);

  // Load a new window with the given name.
  embedder.setUpNewWindowRequest_(webview, 'about:blank', '', testName);
}

function testNewWindowOpenInNewTab() {
  var webview = embedder.setUpGuest_('foobar');

  webview.addEventListener('loadstop', function(e) {
    webview.focus();
    chrome.test.sendMessage('Loaded');
  });
  webview.addEventListener('newwindow', function(e) {
    embedder.test.succeed();
  });
  webview.src = embedder.guestWithLinkURL;
}

// Tests that if opener <webview> is gone with unattached guest, we
// don't see any error.
// This test also makes sure we destroy the unattached guests properly.
function testNewWindowOpenerDestroyedWhileUnattached() {
  var testName = 'testNewWindowOpenerDestroyedBeforeAttach';
  var webview = embedder.setUpGuest_('foobar');

  var onNewWindow = function(e) {
    embedder.assertCorrectEvent_(e, '');

    // Remove the opener.
    webview.parentNode.removeChild(webview);
    // Pass in a timeout so we ensure the newwindow disposal codepath
    // works properly.
    window.setTimeout(function() { embedder.test.succeed(); }, 0);

    e.preventDefault();
  };
  webview.addEventListener('newwindow', onNewWindow);

  // Load a new window with the given name.
  embedder.setUpNewWindowRequest_(webview, 'guest.html', '', testName);
}

function testNewWindowWebRequest() {
  var testName = 'testNewWindowWebRequest';
  var webview = embedder.setUpGuest_('foobar');

  var onNewWindow = function(e) {
    chrome.test.log('Embedder notified on newwindow');
    embedder.assertCorrectEvent_(e, '');

    var newwebview = new WebView();
    var calledWebRequestEvent = false;
    newwebview.request.onBeforeRequest.addListener(function(e) {
      if (!calledWebRequestEvent) {
        calledWebRequestEvent = true;
        embedder.test.succeed();
      }
    }, {urls: ['<all_urls>']});
    document.querySelector('#webview-tag-container').appendChild(newwebview);
    e.preventDefault();
    try {
      e.window.attach(newwebview);
    } catch (e) {
      embedder.test.fail();
    }
  };
  webview.addEventListener('newwindow', onNewWindow);

  // Load a new window with the given name.
  embedder.setUpNewWindowRequest_(webview, 'guest.html', '', testName);
}

// This test verifies that declarative rules added prior to new window
// attachment apply correctly.
function testNewWindowDeclarativeWebRequest() {
  var testName = 'testNewWindowWebRequest';
  var webview = embedder.setUpGuest_('foobar');

  var onNewWindow = function(e) {
    chrome.test.log('Embedder notified on newwindow');
    embedder.assertCorrectEvent_(e, '');

    var newwebview = new WebView();
    var rule = {
      conditions: [
        new chrome.webViewRequest.RequestMatcher(
          {
            url: { urlContains: 'guest' }
          }
        )
      ],
      actions: [
        new chrome.webViewRequest.CancelRequest()
      ]
    };
    newwebview.request.onRequest.addRules([rule]);
    newwebview.addEventListener('loadabort', function(e) {
      embedder.test.assertEq('ERR_BLOCKED_BY_CLIENT', e.reason);
      embedder.test.succeed();
    });
    document.querySelector('#webview-tag-container').appendChild(newwebview);
    e.preventDefault();
    try {
      e.window.attach(newwebview);
    } catch (e) {
      embedder.test.fail();
    }
  };
  webview.addEventListener('newwindow', onNewWindow);

  // Load a new window with the given name.
  embedder.setUpNewWindowRequest_(webview, 'guest.html', '', testName);
}

// This test verifies that a WebRequest event listener's lifetime is not
// tied to the context in which it was created but instead at least the
// lifetime of the embedder window to which it was attached.
function testNewWindowWebRequestCloseWindow() {
  var current = chrome.app.window.current();
  var requestCount = 0;
  var dataUrl = 'data:text/html,<body>foobar</body>';

  var webview = new WebView();
  webview.request.onBeforeRequest.addListener(function(e) {
    console.log('url: ' + e.url);
    ++requestCount;
    if (requestCount == 1) {
      // Close the existing window.
      // TODO(fsamuel): This is currently broken and this test is disabled.
      // Once we close the first window, the context in which the <webview> was
      // created is gone and so the <webview> is no longer a custom element.
      current.close();
      // renavigate the webview.
      webview.src = embedder.newWindowURL;
    } else if (requestCount == 2) {
      embedder.test.succeed();
    }
  }, {urls: ['<all_urls>']});
  webview.addEventListener('loadcommit', function(e) {
    console.log('loadcommit: ' + e.url);
  });
  webview.src = embedder.guestURL;

  chrome.app.window.create('newwindow.html', {
    width: 640,
    height: 480
  }, function(newwindow) {
    newwindow.contentWindow.onload = function(evt) {
      newwindow.contentWindow.document.body.appendChild(webview);
    };
  });
}

function testNewWindowWebRequestRemoveElement() {
  var testName = 'testNewWindowWebRequestRemoveElement';
  var dataUrl = 'data:text/html,<body>foobar</body>';
  var webview = embedder.setUpGuest_('foobar');
  var calledWebRequestEvent = false;
  webview.request.onBeforeRequest.addListener(function(e) {
    if (e.url == embedder.guestURL && calledWebRequestEvent) {
      embedder.test.succeed();
    }
  }, {urls: ['<all_urls>']});

  var onNewWindow = function(e) {
    chrome.test.log('Embedder notified on newwindow');
    embedder.assertCorrectEvent_(e, '');

    e.preventDefault();
    chrome.app.window.create('newwindow.html', {
      width: 640,
      height: 480
    }, function(newwindow) {
      newwindow.contentWindow.onload = function(evt) {
        var newwebview =
            newwindow.contentWindow.document.querySelector('webview');
        newwebview.request.onCompleted.addListener(function(e) {
          if (!calledWebRequestEvent) {
            calledWebRequestEvent = true;
            newwebview.parentElement.removeChild(newwebview);
            setTimeout(function() {
              webview.src = embedder.guestURL;
            }, 0);
          }
        }, {urls: ['<all_urls>']});
        try {
          e.window.attach(newwebview);
        } catch (e) {
          embedder.test.fail();
        }
      };
    });
  };
  webview.addEventListener('newwindow', onNewWindow);

  // Load a new window with the given name.
  embedder.setUpNewWindowRequest_(webview, 'guest.html', '', testName);
}

function testNewWindowAndUpdateOpener() {
  var testName = 'testNewWindowAndUpdateOpener';
  var webview = embedder.setUpGuest_('foobar');

  // Convert window.open requests into new <webview> tags.
  var onNewWindow = function(e) {
    chrome.test.log('Embedder notified on newwindow');
    embedder.assertCorrectEvent_(e, '');

    var newwebview = document.createElement('webview');
    document.querySelector('#webview-tag-container').appendChild(newwebview);
    newwebview.addEventListener('loadstop', () => {
      // Exit after the opened window loads. The rest of the test is
      // implemented on the C++ side.
      embedder.test.succeed();
    });
    try {
      e.window.attach(newwebview);
    } catch (e) {
      embedder.test.fail();
    }
  };
  webview.addEventListener('newwindow', onNewWindow);

  // Load a new window with the given name.
  embedder.setUpNewWindowRequest_(webview, 'guest.html', '', testName);
}

// This is not a test in and of itself, but a means of creating a webview that
// is left in an unattached state, so that the C++ side can test it in that
// state.
function testNewWindowDeferredAttachmentIndefinitely() {
  let testName = 'testNewWindowDeferredAttachmentIndefinitely';
  let webview = embedder.setUpGuest_('foobar');

  webview.addEventListener('newwindow', (e) => {
    embedder.assertCorrectEvent_(e, '');

    let newwebview = document.createElement('webview');
    try {
      e.window.attach(newwebview);
      embedder.test.succeed();
    } catch (e) {
      embedder.test.fail();
    }

    window.setTimeout(() => {
      document.querySelector('#webview-tag-container').appendChild(newwebview);
    }, 999999999);
  });

  embedder.setUpNewWindowRequest_(webview, 'guest.html', '', testName);
}

// This is not a test in and of itself, but a means of creating a webview that
// is left in an unattached state while its opener webview is also in an
// unattached state, so that the C++ side can test it in that state.
function testDestroyOpenerBeforeAttachment() {
  embedder.test.succeed();

  let webview = new WebView();
  webview.src = embedder.guestOpenOnLoadURL;
  document.body.appendChild(webview);

  // By spinning forever here, we prevent `webview` from completing the
  // attachment process. But since the guest is still created and it calls
  // window.open, we have a situation where two unattached webviews have an
  // opener relationship. The C++ side will test that we can shutdown safely in
  // this case.
  while (true) {}
}

embedder.test.testList = {
  'testNewWindowAttachAfterOpenerDestroyed':
      testNewWindowAttachAfterOpenerDestroyed,
  'testNewWindowAttachInSubFrame': testNewWindowAttachInSubFrame,
  'testNewWindowClose': testNewWindowClose,
  'testNewWindowDeclarativeWebRequest': testNewWindowDeclarativeWebRequest,
  'testNewWindowDeferredAttachment': testNewWindowDeferredAttachment,
  'testNewWindowDiscardAfterOpenerDestroyed':
      testNewWindowDiscardAfterOpenerDestroyed,
  'testNewWindowExecuteScript': testNewWindowExecuteScript,
  'testNewWindowNameTakesPrecedence': testNewWindowNameTakesPrecedence,
  'testNewWindowNoName': testNewWindowNoName,
  'testNewWindowOpenInNewTab': testNewWindowOpenInNewTab,
  'testNewWindowOpenerDestroyedWhileUnattached':
      testNewWindowOpenerDestroyedWhileUnattached,
  'testNewWindowRedirect':  testNewWindowRedirect,
  'testNewWindowWebRequest': testNewWindowWebRequest,
  'testNewWindowWebRequestCloseWindow': testNewWindowWebRequestCloseWindow,
  'testNewWindowWebRequestRemoveElement': testNewWindowWebRequestRemoveElement,
  'testNewWindowWebViewNameTakesPrecedence':
      testNewWindowWebViewNameTakesPrecedence,
  'testNewWindowAndUpdateOpener': testNewWindowAndUpdateOpener,
  'testNewWindowDeferredAttachmentIndefinitely':
      testNewWindowDeferredAttachmentIndefinitely,
  'testDestroyOpenerBeforeAttachment':
      testDestroyOpenerBeforeAttachment
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp_(config);
    chrome.test.sendMessage('Launched');
  });
};
