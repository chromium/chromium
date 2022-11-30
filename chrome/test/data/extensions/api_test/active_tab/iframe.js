// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function canXhr(url) {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', url, false);
  var success = true;
  try {
    xhr.send();
  } catch(e) {
    assertEq('NetworkError', e.name);
    success = false;
  }
  return success;
}

document.addEventListener('DOMContentLoaded', function() {
  chrome.runtime.sendMessage({message:'xhr'}, function(response) {
    var success = canXhr(response.url);
    chrome.runtime.sendMessage({success:success});
  });
});
