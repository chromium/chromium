// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var target = location.href + "#foo";

function updateFragment() {
  location.href = target;
}

onload = function() {
  setTimeout(function() {
    location.href =
        "http://127.0.0.1:" + location.search.substr(1) + "/test3";
  }, 0);
};
