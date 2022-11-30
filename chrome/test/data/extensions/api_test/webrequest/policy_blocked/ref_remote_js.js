// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Injects a script tag with a source based on the current URL. The script
// however will be served from a different domain (example2.com) and target a
// different file which is blank. This lets us make a request with an initiator
// of example.com and a URL of example2.com.
var script = document.createElement('script');
script.src = 'http://example2.com:' + location.port + '/empty.html';
document.body.appendChild(script);
