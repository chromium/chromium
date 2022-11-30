// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  var frame = document.getElementById('frame');
  frame.parentNode.removeChild(frame);
  frame = null;
};
