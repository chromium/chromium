// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.requestFileSystemSync = self.webkitRequestFileSystemSync ||
                             self.requestFileSystemSync;

function onError(e) {
  postMessage({ 'type': 'error', 'msg': e.toString() });
}

function echoMsg(msg) {
  postMessage({'type': 'echo', 'msg': msg});
}

function requestFileSystem() {
  try {
    echoMsg("call requetFileSystem");
    var filesystem = requestFileSystemSync(PERSISTENT, 1024 * 1024 /* 1MB */);
    var result = filesystem ? 1 : 0;
    postMessage({'type': 'result', 'msg': result});
  } catch (e) {
    onError(e);
  }
}

addEventListener('message', function(e) {
  var data = e.data;
  switch(data.type) {
    case 'echo':
      postMessage({'type': 'echo', 'msg': data.msg});
      return;
    case 'requestFileSystem':
      requestFileSystem();
      return;
    default:
      postMessage({'type': 'error', 'msg': 'UNKNOWN MESSAGE TYPE'});
  }
});
