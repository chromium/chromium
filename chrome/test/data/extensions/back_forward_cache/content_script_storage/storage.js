// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.storage.onChanged.addListener((changes, area) => {
  let input = document.createElement("input");
  input.id = "callback"
  input.value += "called";
  document.documentElement.appendChild(input);
});

if (window.location.host.indexOf('b.com') != -1) {
  var options = {test: true};
  chrome.storage.sync.set({options});
}