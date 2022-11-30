// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.addEventListener('DOMContentLoaded', function() {
  var body = document.body;
  var div = body.appendChild(document.createElement('div'));
  var input1 = body.appendChild(document.createElement('input'));
  var input2 = body.appendChild(document.createElement('input'));

  input1.focus();
  input1.addEventListener('blur', function() {
    div.setAttribute('baz', 'bat');
  });

  var success = false;
  var mutationsDelivered = false;

  var observer = new MutationObserver(function() {
    mutationsDelivered = true;
    if (success)
      chrome.test.succeed();
  });
  observer.observe(document, { subtree: true, attributes: true });

  // The getAll callback should be counted as a V8RecursionScope and cause
  // the delivery of MutationRecords to be delayed until it has exited.
  chrome.windows.getAll(function() {
    div.setAttribute('foo', 'bar');
    input2.focus();
    if (mutationsDelivered)
      chrome.test.fail();
    else
      success = true;
  });
});
