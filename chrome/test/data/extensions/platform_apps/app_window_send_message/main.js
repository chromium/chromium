// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  var iframe = document.createElement('iframe');
  iframe.src = 'data:text/html,<body>hello world.</body>';
  document.body.appendChild(iframe);
};
