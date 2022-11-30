// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = null;

function reportConnected_request() {
  window.console.log('Reporting connection to embedder.');
  var msg = ['connected_response'];
  embedder.postMessage(JSON.stringify(msg), '*');
}

window.addEventListener('message', function(e) {
  embedder = e.source;
  var data = JSON.parse(e.data);
  switch (data[0]) {
    case 'connect_request': {
      reportConnected_request();
      break;
    }
  }
});


window.console.log('Script has been successfully injected.');
