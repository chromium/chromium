// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var port = chrome.extension.connect({name: 'continue_propagation'});
document.body.addEventListener('keyup', function(evt) {
  if (evt.keyCode == 70 /* F */)
    port.postMessage({result: true});
  }, true);
