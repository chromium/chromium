// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = null;

function reportConnected() {
  window.console.log('Reporting connection to embedder.');
  var msg = ['connected'];
  embedder.postMessage(JSON.stringify(msg), '*');
}

window.addEventListener('message', function(e) {
  embedder = e.source;
  var data = JSON.parse(e.data);
  switch (data[0]) {
    case 'connect': {
      reportConnected();
      break;
    }
  }
});


window.console.log('Script has been successfully injected.');
