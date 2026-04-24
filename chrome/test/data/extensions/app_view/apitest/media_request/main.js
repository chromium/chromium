// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  var webview = document.createElement("webview");
  webview.partition = "media";
  webview.addEventListener("permissionrequest", function(e) {
    if (e.permission == "media") {
      e.request.allow();
    }
  });
  document.body.appendChild(webview);
  webview.src = "guest.html";
}
