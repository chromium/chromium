// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  setTimeout(function() {
    var target =
        "http://127.0.0.1:" +
        location.search.substr(1) +
        "/extensions/api_test/webnavigation/crossProcess/empty.html";
    location.href =
        "http://www.a.com:" +
        location.search.substr(1) +
        "/server-redirect?" + target;
  }, 0);
};
