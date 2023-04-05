// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function createAdFencedFrame(url, name) {
  const frame = document.createElement('fencedframe');
  const config = new FencedFrameConfig(url);
  frame.name = name;
  frame.id = name;
  frame.config = config;
  document.body.appendChild(frame);
}

function createAdFrame(url, name, sbox_attr, load_callback, error_callback) {
  const frame = document.createElement('iframe');
  frame.name = name;
  frame.id = name;
  frame.src = url;
  if (load_callback !== undefined) {
    frame.onload = load_callback;
  }
  if (error_callback !== undefined) {
    frame.onerror = error_callback;
  }
  if (sbox_attr !== undefined) {
    frame.sandbox = sbox_attr;
  }
  document.body.appendChild(frame);
}

function createAdFramePromise(url, name, sbox_attr) {
  return new Promise((resolve, reject) => {
    createAdFrame(url, name, sbox_attr, resolve, reject);
  });
}

function windowOpenFromAdScript(url) {
  window.open(url);
}

function navigateIframeFromAdScript(name, url) {
  document.getElementsByName(name)[0].src = url;
}

async function createDocWrittenAdFrame(name, base_url) {
  const docBody = await fetch('frame_factory.html');
  const docText = await docBody.text();

  const frame = document.createElement('iframe');
  frame.name = name;
  document.body.appendChild(frame);

  frame.contentDocument.open();
  return new Promise(resolve => {
    frame.onload = function() {
      resolve(true);
    };
    frame.contentDocument.write(docText);
    frame.contentDocument.close();
  });
}

function createAdFrameWithDocWriteAbortedLoad(name) {
  const frame = document.createElement('iframe');
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
  const frame = document.createElement('iframe');
  frame.name = name;

  // slow takes 100 seconds to load, plenty of time to overwrite the
  // provisional load.
  frame.src = '/slow?100';
  document.body.appendChild(frame);
  frame.contentWindow.stop();

  // We load the scripts in frame_factory.html to allow subframe creation. We
  // set the async attribute to false to ensure that these scripts are loaded in
  // insertion order.
  const script1 = document.createElement('script');
  script1.async = false;
  script1.src = 'create_frame.js';
  frame.contentDocument.head.appendChild(script1);

  const script2 = document.createElement('script');
  script2.async = false;
  script2.src = 'ad_script.js';
  frame.contentDocument.head.appendChild(script2);

  const script3 = document.createElement('script');
  script3.async = false;
  script3.src = 'ad_script_2.js';
  // Set title so we know when all scripts have loaded.
  script3.onload = function() {
    top.document.title = name;
  };
  frame.contentDocument.head.appendChild(script3);
}
