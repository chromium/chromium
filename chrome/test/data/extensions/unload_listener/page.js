// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onUnload() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', 'https://www.example.com/', true);
  xhr.send();
}

window.onbeforeunload = onUnload;
window.onunload = onUnload;
