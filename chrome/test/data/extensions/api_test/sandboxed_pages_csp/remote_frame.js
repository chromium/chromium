// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onmessage = function(e) {
  var data = JSON.parse(e.data);
  if (data[0] != 'sandboxed frame msg')
    return;
  var param = data[1];
  e.source.postMessage(JSON.stringify(['remote frame msg', param]), '*');
};
