// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.info('start guest js');

let embedder = null;
window.addEventListener('message', function(e) {
  const data = JSON.parse(e.data)[0];
  window.console.info('guest gets message ' + data);
  if (data === 'create-channel') {
    embedder = e.source;
    doPostMessage('connected');
  }
});

function doPostMessage(msg) {
  window.console.info('guest posts message: ' + msg);
  embedder.postMessage(JSON.stringify([msg]), '*');
}

document.body.style.background = '#EEEEEE';
document.body.innerHTML +=
    '<input class="destination" id="dest">destination</input>';

const destNode = document.getElementById('dest');
let testStep = 0;
destNode.addEventListener('dragenter', function(e) {
  console.info('node drag enter');
  if (testStep === 0) {
    doPostMessage('Step1: destNode gets dragenter');
    testStep = 1;
  }
});

destNode.addEventListener('dragover', function(e) {
  if (testStep === 1) {
    doPostMessage('Step2: destNode gets dragover');
    testStep = 2;
  }
});

destNode.addEventListener('drop', function(e) {
  if (testStep === 2) {
    doPostMessage('Step3: destNode gets drop');
    testStep = 3;
  }
});

console.info('finish guest js');
