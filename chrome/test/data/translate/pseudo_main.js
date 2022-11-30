// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

div_log = function (message) {
  // console.log() does not work inside the isolated world.
  // Add DOM elements to show error messages.
  var div = document.getElementById('log');
  var element = document.createElement('pre');
  element.innerText = message;
  div.appendChild(element);
}
if (typeof world != 'undefined') {
  div_log('v8 isolation doesn\'t work');
  document.title = 'FAIL';
} else {
  div_log('The first script pseudo_main.js is loaded');
  div_log('Loading the second script pseudo_element_main.js ...');
  cr.googleTranslate.onLoadJavascript(element_main_script_url);
}
