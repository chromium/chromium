// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var eFrame = document.createElement('iframe');
eFrame.onload = function() {
  var fFrame = document.createElement('iframe');
  fFrame.src = 'f.html';
  document.body.appendChild(fFrame);
};
eFrame.src = 'e.html';
document.body.appendChild(eFrame);
