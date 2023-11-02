// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var swIframeHTMLResponse =
    '<script src="iframe_sw.js"></script>';

var swIframeNonExistentHTMLResponse =
    '<script src="iframe_non_existent.js"></script>';

var swIframeJSResponse =
    'window.onmessage = function(e) {' +
    '  if (e.data == "ping") {' +
    '    e.source.postMessage("FROM_SW_RESOURCE", "*")' +
    '  }' +
    '};';

self.onfetch = function(e) {
  var url = new URL(e.request.url);
  if (url.pathname == '/iframe.html') {
    e.respondWith(new Response(swIframeHTMLResponse, {
      headers: {'Content-Type': 'text/html'}
    }));
  } else if (url.pathname == '/iframe_non_existent.html') {
    e.respondWith(new Response(swIframeNonExistentHTMLResponse, {
      headers: {'Content-Type': 'text/html'}
    }));
  } else if (url.pathname == '/iframe_sw.js' ||
             url.pathname == '/iframe_non_existent.js') {
    e.respondWith(new Response(swIframeJSResponse));
  }
};
