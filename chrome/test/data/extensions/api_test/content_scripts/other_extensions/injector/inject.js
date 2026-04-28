// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.info('INJECTOR: Injecting content script!');

const content = document.getElementById('content');
if (content) {
  content.innerText = 'Injected!!!';
  console.info('INJECTOR: Changed content to: ' + content.innerText);
} else {
  console.info('INJECTOR: Cannot find content!?');
}
