// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var iframe_hosts = ['https://127.0.0.1', 'https://localhost'];
function getIFrameSrc(iframe_id) {
  var port = location.port;
  var path = location.pathname.substring(0, location.pathname.lastIndexOf('/'));
  var url = iframe_hosts[iframe_id] + ':' + port + path + '/simple.html';
  return url;
}
function addIFrame(iframe_id, iframe_src) {
  var id = 'iframe_' + iframe_id;
  var iframe = document.getElementById(id);
  iframe.allow = 'geolocation';
  if (iframe_src) {
    iframe.src = iframe_src;
  } else {
    iframe.src = getIFrameSrc(iframe_id);
  }
  return "" + iframe_id;
}
