// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.diceResponseHeaderCount = 0;
self.controlResponseHeaderCount = 0;

chrome.webRequest.onHeadersReceived.addListener(function(details) {
  let diceHeaderFound = false;
  const headerValue = 'ValueFromExtension'
  const diceResponseHeader = 'X-Chrome-ID-Consistency-Response';
  details.responseHeaders.forEach(function(header) {
    if (header.name == diceResponseHeader){
      ++self.diceResponseHeaderCount;
      diceHeaderFound = true;
      header.value = headerValue;
    } else if (header.name == 'X-Control'){
      ++self.controlResponseHeaderCount;
      header.value = headerValue;
    }
  });
  if (!diceHeaderFound) {
    details.responseHeaders.push({name: diceResponseHeader,
                                  value: headerValue});
  }
  details.responseHeaders.push({name: 'X-New-Header',
                                value: headerValue});
  return {responseHeaders: details.responseHeaders};
},
{urls: ['http://*/extensions/dice.html']},
['blocking', 'responseHeaders']);

chrome.test.sendMessage('ready');
