// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  // We expect three events: PENDING, IN_PROGRESS and CANCELED as the final
  // one.
  const statuses = [
    chrome.printing.JobStatus.PENDING, chrome.printing.JobStatus.IN_PROGRESS,
    chrome.printing.JobStatus.CANCELED
  ];
  var eventCounter = 0;
  chrome.printing.onJobStatusChanged.addListener((jobId, status) => {
    chrome.test.assertEq(statuses[eventCounter], status);
    eventCounter++;
    // We don't expect any other events to happen so finish the test as
    // passed.
    if (eventCounter == statuses.length)
      chrome.test.notifyPass();
  });

  const url = 'http://localhost:' + config.testServer.port + '/pdf/test.pdf';
  submitJob('id', 'test job', url, response => {
    chrome.test.assertTrue(response != undefined);
    chrome.test.assertTrue(response.status != undefined);
    chrome.test.assertEq(chrome.printing.SubmitJobStatus.OK, response.status);
    chrome.test.assertTrue(response.jobId != undefined);

    chrome.printing.cancelJob(response.jobId, () => {});
  });
});
