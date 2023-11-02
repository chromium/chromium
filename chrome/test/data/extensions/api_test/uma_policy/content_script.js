// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.write('<div id="hi">hello</div>');

document.getElementById('hi').innerHTML = 'innerhtml';

document.createElement('script');
document.createElement('iframe');
document.createElement('div');
document.createElement('a');
document.createElement('input');
document.createElement('embed');
document.createElement('object');

var endConditions = {};
endConditions['http://www.google.com/'] = true;
endConditions['http://www.google.com/?q=a'] = true;
endConditions['http://www.google.com/?q=b'] = true;
endConditions['http://www.cnn.com/?q=a'] = true;
endConditions['http://www.cnn.com/?q=b'] = true;

var doubleEndConditions = {};
doubleEndConditions['http://www.google.com/?p=a'] = true;
doubleEndConditions['http://www.google.com/?p=b'] = true;
doubleEndConditions['http://www.cnn.com/#a'] = true;
doubleEndConditions['http://www.cnn.com/#b'] = true;
doubleEndConditions['http://www.google.com/#a'] = true;
doubleEndConditions['http://www.google.com/#b'] = true;

if (document.location in endConditions) {
  chrome.runtime.sendMessage({testType: 'single'}, function(response) {});
} else if (document.location in doubleEndConditions) {
  chrome.runtime.sendMessage({testType: 'double'}, function(response) {});
}

window.close();
