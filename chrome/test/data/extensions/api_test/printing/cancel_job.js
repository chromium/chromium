// Copyright 2020 The Chromium Authors
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

    if (status == chrome.printing.JobStatus.IN_PROGRESS) {
      chrome.printing.cancelJob(jobId, () => {});
    }
  });

  const url = 'http://localhost:' + config.testServer.port + '/pdf/test.pdf';
  submitJob('id', 'test job', url, response => {
    chrome.test.assertNe(undefined, response);
    chrome.test.assertNe(undefined, response.status);
    chrome.test.assertEq(chrome.printing.SubmitJobStatus.OK, response.status);
    chrome.test.assertNe(undefined, response.jobId);
  });
});
