// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const eFrame = document.createElement('iframe');
eFrame.onload = function() {
  const fFrame = document.createElement('iframe');
  fFrame.src = 'f.html';
  document.body.appendChild(fFrame);
};
eFrame.src = 'e.html';
document.body.appendChild(eFrame);
