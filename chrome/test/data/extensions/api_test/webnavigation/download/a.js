// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  var port = location.search.substr(1);
  var redirect =
      "http://127.0.0.1:" + port + "/server-redirect";
  var target =
      "http://127.0.0.1:" + port + "/not-found";

  var link = document.createElement("a");
  link.href = redirect + "?" + target;
  link.download = "somefile.txt";
  document.body.appendChild(link);
  link.click();
};
