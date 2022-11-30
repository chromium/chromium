// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var target =
    location.origin + location.pathname.replace("d.html", "empty.html");

onload = function() {
  setTimeout(function() {
    location.href =
        "http://127.0.0.1:" + location.search.substr(1);
  }, 0);
};

function navigate2() {
  location.href = target;
}
