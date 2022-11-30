// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var action_counter = 0;

chrome.rtcPrivate.onLaunch.addListener(function(launchData) {
  if(!launchData) {
    console.log('HANDLER: got default action');
    action_counter++;
    return;
  }

  var action = launchData.intent.action;
  if (action == 'chat' || action == 'video' || action == 'voice') {
    console.log('HANDLER: Received ' + action +
      ', data = ' + launchData.intent.data);
    var data = launchData.intent.data;
    var type = launchData.intent.type;
    if (type != 'application/vnd.chromium.contact' ||
        data.name != 'Test Contact' ||
        data.phone.length != 2 ||
        data.phone[0] != '(555) 111-2222' ||
        data.phone[1] != '(555) 333-4444' ||
        data.email.length != 2 ||
        data.email[0] != 'test_1@something.com' ||
        data.email[1] != 'test_2@something.com') {
      chrome.test.fail('HANDLER: Invalid data!');
      return;
    }
    action_counter++;
    if (action_counter == 4)
      chrome.test.sendMessage('received_all');
  } else {
    console.log('HANDLER: unknown action - ' + action);
    chrome.test.fail('HANDLER: No content changed!');
    return;
  }
});
