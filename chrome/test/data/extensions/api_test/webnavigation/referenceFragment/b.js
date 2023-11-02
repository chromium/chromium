// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.getElementById('btn').addEventListener('click', function() {
  location.replace(location.href);
});

onload = function() {
  // Set a sentinel value in sessionStorage to avoid clicking the button on
  // the second load of the document. Otherwise a second click will dispatch
  // another fragment navigation, which the test would not expect.
  if (sessionStorage['foo'])
    return;
  sessionStorage['foo'] = true;

  setTimeout(function() {
    document.getElementById('btn').click();
  }, 0);
}
