// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
const scriptMessageEvent = document.createEvent('Event');
scriptMessageEvent.initEvent('scriptMessage', true, true);

const pageToScriptTunnel = document.getElementById('pageToScriptTunnel');
pageToScriptTunnel.addEventListener('scriptMessage', function() {
  const data = JSON.parse(pageToScriptTunnel.innerText);
  chrome.runtime.sendMessage(data);
});

chrome.runtime.onMessage.addListener(function(request) {
  const scriptToPageTunnel = document.getElementById('scriptToPageTunnel');
  scriptToPageTunnel.innerText = JSON.stringify(request);
  scriptToPageTunnel.dispatchEvent(scriptMessageEvent);
});
