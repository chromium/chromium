// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const redDot = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUA' +
    'AAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO' +
    '9TXL0Y4OHwAAAABJRU5ErkJggg==';

const options = {
        iconUrl: redDot,
        title: 'hi',
        message: 'there',
        type: 'basic',
        appIconMaskUrl: redDot,
        buttons: [{title: 'Button'}, {title: 'Button'}]
      };

onload = function() {
  chrome.notifications.create('test', options, function() {
    chrome.test.sendMessage('created');
  });
};
