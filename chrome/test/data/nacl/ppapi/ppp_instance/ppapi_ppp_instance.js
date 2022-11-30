// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function startsWith(str, prefix) {
  return (str.indexOf(prefix) === 0);
}

function setupTests(tester, plugin) {
  //////////////////////////////////////////////////////////////////////////////
  // Test Helpers
  //////////////////////////////////////////////////////////////////////////////
  var numMessages = 0;
  function addTestListeners(numListeners, test, testFunction, runCheck) {
    var messageListener = test.wrap(function(message) {
      if (!startsWith(message.data, testFunction)) return;
      test.log(message.data);
      numMessages++;
      plugin.removeEventListener('message', messageListener, false);
      test.assertEqual(message.data, testFunction + ':PASSED');
      if (runCheck) test.assert(runCheck());
      if (numMessages < numListeners) {
        plugin.addEventListener('message', messageListener, false);
      } else {
        numMessages = 0;
        test.pass();
      }
    });
    plugin.addEventListener('message', messageListener, false);
  }

  function addTestListener(test, testFunction, runCheck) {
    return addTestListeners(1, test, testFunction, runCheck);
  }

  //////////////////////////////////////////////////////////////////////////////
  // Tests
  //////////////////////////////////////////////////////////////////////////////

  tester.addTest('PPP_Instance::DidCreate', function() {
    assertEqual(plugin.lastError, '');
  });

  tester.addAsyncTest('PPP_Instance::DidChangeView', function(test) {
    // The .cc file hardcodes an expected 15x20 size.
    plugin.width = 15;
    plugin.height = 20;
    addTestListener(test, 'DidChangeView');
  });

  // This test does not appear to be reliable on the bots.
  // http://crbug.com/329511
  /*
  tester.addAsyncTest('PPP_Instance::DidChangeFocus', function(test) {
    // TODO(polina): How can I simulate focusing on Windows?
    // For now just pass explicitely.
    if (startsWith(navigator.platform, 'Win')) {
      test.log('skipping test on ' + navigator.platform);
      test.pass();
      return;
    }
    addTestListeners(2, test, 'DidChangeFocus');
    plugin.tabIndex = 0;
    plugin.focus();
    plugin.blur();
  });
  */

  // PPP_Instance::HandleDocumentLoad is only used with full-frame plugins.
  // This is tested in tests/ppapi_browser/extension_mime_handler/

  // PPP_Instance::DidDestroy is never invoked in the untrusted code.
  // We could wait for a crash event from it, but CallOnMainThread semantics
  // on shutdown are still buggy, so it might never come even if the function
  // triggered. Plus waiting for something not to happen makes the test flaky.
}
