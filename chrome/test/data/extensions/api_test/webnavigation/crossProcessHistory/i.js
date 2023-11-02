// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function updateHistory() {
  history.replaceState({}, "", "empty.html");
}

onload = function() {
  setTimeout(function() {
    location.href =
        "http://127.0.0.1:" + location.search.substr(1) + "/test6";
  }, 0);
};
