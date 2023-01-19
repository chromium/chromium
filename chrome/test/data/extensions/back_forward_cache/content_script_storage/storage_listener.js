// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.storage.onChanged.addListener((changes, area) => {
  let input = document.createElement("input");
  input.id = "callback"
  input.value += "called";
  document.documentElement.appendChild(input);
  // Let the test know that we ran.
  window.domAutomationController.send("event handler ran");
});
