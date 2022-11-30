// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = null;

function reportResult(result) {
  var msg = ['fetch-' + result];
  embedder.postMessage(JSON.stringify(msg), '*');
}

window.addEventListener('message', function(e) {
  embedder = e.source;
  var data = JSON.parse(e.data);
  if (data[0] == 'start-fetch') {
    fetch('http://127.0.0.1:' + location.port + '/extensions/xhr.txt')
        .then((result) => result.text())
        .then((text) => {
          reportResult(
              (text == 'File to request via XHR.\n') ? 'success' : 'failure');
        })
        .catch(err => {
          reportResult('failure');
        });
  } else {
    reportResult('unexpected-message');
  }
});

window.console.log('Script has been successfully injected.');
