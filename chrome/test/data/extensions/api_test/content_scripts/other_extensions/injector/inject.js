// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log('INJECTOR: Injecting content script!');

var content = document.getElementById('content');
if (content) {
  content.innerText = 'Injected!!!';
  console.log('INJECTOR: Changed content to: ' + content.innerText);
} else {
  console.log('INJECTOR: Cannot find content!?');
}
