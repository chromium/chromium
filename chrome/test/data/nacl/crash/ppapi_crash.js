// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper routines for generating crash load tests.
// Depends on nacltest.js.

function createModule(id, type) {
  return createNaClEmbed({
    id: id,
    src: 'ppapi_' + id + '.nmf',
    width: 1,
    height: 1,
    type: type
  });
}


function crashTest(pluginName, testName, pingToDetectCrash) {
  var mime = 'application/x-nacl';
  if (getTestArguments()['pnacl'] !== undefined) {
    mime = 'application/x-pnacl';
  }

  var plugin = createModule(pluginName, mime);
  document.body.appendChild(plugin);

  var tester = new Tester();
  tester.addAsyncTest(testName, function(test) {
    test.expectEvent(plugin, 'crash', function(e) { test.pass(); });
    test.expectEvent(plugin, 'error', function(e) { test.fail(); });
    plugin.postMessage(testName);
    // In case the nexe does not crash right away, we will ping it
    // until we detect that it's dead. DidChangeView and other events
    // can do this too, but are less reliable.
    if (pingToDetectCrash) {
      function PingBack(message) {
        test.log(message.data);
        plugin.postMessage('Ping');
      }
      plugin.addEventListener('message', PingBack, false);
      plugin.postMessage("Ping");
    }
  });
  tester.waitFor(plugin);
  tester.run();
}
