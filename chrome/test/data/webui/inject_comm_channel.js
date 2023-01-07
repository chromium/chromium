// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-var
var embedder = null;

function reportConnected() {
  const msg = ['connected'];
  embedder.postMessage(JSON.stringify(msg), '*');
}

window.addEventListener('message', function(e) {
  embedder = e.source;
  const data = JSON.parse(e.data);
  switch (data[0]) {
    case 'connect': {
      reportConnected();
      break;
    }
  }
});
