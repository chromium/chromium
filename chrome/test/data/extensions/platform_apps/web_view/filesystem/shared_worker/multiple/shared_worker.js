// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.requestFileSystemSync = self.webkitRequestFileSystemSync ||
                             self.requestFileSystemSync;

addEventListener('connect', function(e) {
  var port = e.ports[0];
  function onError(e) {
    port.postMessage({ 'type': 'error', 'msg': e.toString() });
  }

  function echoMsg(msg) {
    port.postMessage({'type': 'echo', 'msg': msg});
  }

  function requestFileSystem() {
    try {
      echoMsg("call requetFileSystem");
      var filesystem = requestFileSystemSync(PERSISTENT, 1024 * 1024 /* 1MB */);
      var result = filesystem ? 1 : 0;
      port.postMessage({'type': 'result', 'msg': result});
    } catch (e) {
      onError(e);
    }
  }

  port.addEventListener('message', function(e) {
    var data = e.data;
    switch(data.type) {
      case 'echo':
        port.postMessage({'type': 'echo', 'msg': data.msg});
        return;
      case 'requestFileSystem':
        requestFileSystem();
        return;
      default:
        port.postMessage({'type': 'error', 'msg': 'UNKNOWN MESSAGE TYPE'});
    }
  });
  port.start();
});
