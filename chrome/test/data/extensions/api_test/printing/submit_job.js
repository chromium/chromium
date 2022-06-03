// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  const url = 'http://localhost:' + config.testServer.port + '/pdf/test.pdf';
  submitJob('id', 'test job', url, response => {
    chrome.test.assertTrue(response != undefined);
    chrome.test.assertTrue(response.status != undefined);
    chrome.test.assertEq(chrome.printing.SubmitJobStatus.OK, response.status);
    chrome.test.assertTrue(response.jobId != undefined);

    chrome.test.notifyPass();
  });
});
