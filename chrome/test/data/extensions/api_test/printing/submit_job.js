// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  const url = 'http://localhost:' + config.testServer.port + '/pdf/test.pdf';
  submitJob('id', 'test job', url, response => {
    chrome.test.assertNe(undefined, response);
    chrome.test.assertNe(undefined, response.status);
    chrome.test.assertEq(chrome.printing.SubmitJobStatus.OK, response.status);
    chrome.test.assertNe(undefined, response.jobId);

    chrome.test.notifyPass();
  });
});
