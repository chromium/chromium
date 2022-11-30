// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addAppViewForExtensionId(guestExtensionId, callback) {
  var appview = document.createElement('appview');
  document.getElementById('embedder').appendChild(appview);
  appview.connect(guestExtensionId, {}, function(success) {
    callback(success);
  });
}

window.onAppCommand = function(cmd, param) {
  if (cmd === 'EMBED') {
    addAppViewForExtensionId(param, function(success) {});
  }
};
