// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var iframe = document.getElementById('iframe');

function printIframe() {
  iframe.contentWindow.print();
}

if (iframe.contentDocument && iframe.contentDocument.body &&
    iframe.contentDocument.body.firstChild) {
  printIframe();
} else {
  iframe.onload = printIframe;
}
