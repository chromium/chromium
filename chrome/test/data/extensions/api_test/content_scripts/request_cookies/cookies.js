// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let requests = [
  {method: 'GET', url: '/set-cookie?laxcookie=1;samesite=lax'},
  {method: 'GET', url: '/set-cookie?strictcookie=1;samesite=strict'},
  {
    method: 'GET',
    url: '/expect-and-set-cookie?expect=laxcookie%3d1&set=laxFoundGet%3d1'
  },
  {
    method: 'GET',
    url:
        '/expect-and-set-cookie?expect=strictcookie%3d1&set=strictFoundGet%3d1'
  },
  {
    method: 'POST',
    url:
        '/expect-and-set-cookie?expect=laxcookie%3d1&set=laxFoundPost%3d1'
  },
  {
    method: 'POST',
    url:
        '/expect-and-set-cookie?expect=strictcookie%3d1&set=strictFoundPost%3d1'
  }
];

(async function makeRequests() {
  // Make all the requests to the server in a sequential order.
  for (const req of requests)
    await fetch(req.url, {method: req.method, credentials: 'same-origin'});

  // Verify that the expected cookies were seen on the server side and the
  // expected cookies in response are present.
  let s = document.cookie.split('; ');
  if (s.includes('laxFoundGet=1') &&
      s.includes('strictFoundGet=1') &&
      s.includes('laxFoundPost=1') &&
      s.includes('strictFoundPost=1')) {
    chrome.test.notifyPass();
  }

  chrome.test.notifyFail(document.cookie);
})();
