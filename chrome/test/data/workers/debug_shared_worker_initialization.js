// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

debugger;
var globalVar = 2011;
onconnect = function(e) {
  var port = e.ports[0];
  console.log('connected');
  port.postMessage("pong");
}

