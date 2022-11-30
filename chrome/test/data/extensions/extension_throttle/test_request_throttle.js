// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const maxRequests = 3;

var searchParams = new URLSearchParams(location.search);
var url = searchParams.get('url');
var requestsToMake;
var expectedFailRequestNum;
if (searchParams.has('expectedFailRequestNum')) {
  expectedFailRequestNum = parseInt(searchParams.get('expectedFailRequestNum'));
  requestsToMake = expectedFailRequestNum;
} else {
  expectedFailRequestNum = maxRequests + 1;
  requestsToMake = maxRequests;
}

chrome.runtime.sendMessage({type: 'xhr', method: 'GET', url: url,
                            requestsToMake: requestsToMake,
                            expectedFailRequestNum: expectedFailRequestNum});
