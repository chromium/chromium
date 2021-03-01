// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function createAdFrame(url, name, sbox_attr) {
  let frame = document.createElement('iframe');
  frame.name = name;
  frame.id = name;
  frame.src = url;
  if (sbox_attr !== undefined) {
    frame.sandbox = sbox_attr;
  }
  document.body.appendChild(frame);
}

function windowOpenFromAdScript() {
  window.open();
}

async function createDocWrittenAdFrame(name, base_url) {
  let doc_body = await fetch('frame_factory.html');
  let doc_text = await doc_body.text();

  let frame = document.createElement('iframe');
  frame.name = name;
  document.body.appendChild(frame);

  frame.contentDocument.open();
  frame.onload = function() {
    window.domAutomationController.send(true);
  }
  frame.contentDocument.write(doc_text);
  frame.contentDocument.close();
}

function createAdFrameWithDocWriteAbortedLoad(name) {
  let frame = document.createElement('iframe');
  frame.name = name;

  // slow takes 100 seconds to load, plenty of time to overwrite the
  // provisional load.
  frame.src = '/slow?100';
  document.body.appendChild(frame);
  frame.contentDocument.open();
  // We load the scripts in frame_factory.html to allow subframe creation,
  // setting the title so we know when all scripts have loaded.
  frame.contentDocument.write(
      '<html><head>' +
      '<script src="create_frame.js"></script>' +
      '<script src="ad_script.js"></script>' +
      '<script onload="top.document.title = window.name" ' +
      'src="ad_script_2.js"></script></head><body></body></html>');
  frame.contentDocument.close();
}

function createAdFrameWithWindowStopAbortedLoad(name) {
  let frame = document.createElement('iframe');
  frame.name = name;

  // slow takes 100 seconds to load, plenty of time to overwrite the
  // provisional load.
  frame.src = '/slow?100';
  document.body.appendChild(frame);
  frame.contentWindow.stop();

  // We load the scripts in frame_factory.html to allow subframe creation. We
  // set the async attribute to false to ensure that these scripts are loaded in
  // insertion order.
  let script1 = document.createElement('script');
  script1.async = false;
  script1.src = 'create_frame.js';
  frame.contentDocument.head.appendChild(script1);

  let script2 = document.createElement('script');
  script2.async = false;
  script2.src = 'ad_script.js';
  frame.contentDocument.head.appendChild(script2);

  let script3 = document.createElement('script');
  script3.async = false;
  script3.src = 'ad_script_2.js';
  // Set title so we know when all scripts have loaded.
  script3.onload = function() {
    top.document.title = name;
  };
  frame.contentDocument.head.appendChild(script3);
}
