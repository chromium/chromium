// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create("window.html", {}, function() {
  });
});


chrome.app.runtime.onEmbedRequested.addListener(function(request) {
  console.log('Embed request received at the guest app.');
  window.request = request;
  chrome.test.sendMessage('AppViewTest.EmbedRequested');
});

window.continueEmbedding = function() {
  console.log('Moving on with the embedding.');
  if (window.request) {
    request.allow('window.html');
  }
}

