// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var input = document.getElementById('stage');
if (!input) {
  input = document.createElement("input");
  input.id = "stage";
  document.documentElement.appendChild(input);
}
input.value += "document_start/";

window.addEventListener('pagehide', () => {
  input.value += "page_hide/";
});

window.addEventListener('pageshow', () => {
  input.value += "page_show/";
});
