// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
    chrome.printing.getJobStatus('invalid_job_id', status => {
      chrome.test.assertEq(null, status);
    });

    const url = 'http://localhost:' + config.testServer.port + '/pdf/test.pdf';
    submitJob('id', 'test job', url, minimal_ticket, response => {
      chrome.test.assertNe(undefined, response);
      chrome.test.assertNe(undefined, response.status);
      chrome.test.assertEq(chrome.printing.SubmitJobStatus.OK, response.status);
      chrome.test.assertNe(undefined, response.jobId);

      chrome.printing.getJobStatus('id', status => {
        chrome.test.assertEq(chrome.printing.JobStatus.COMPLETED, status);

        chrome.test.notifyPass();
      });
    });
  });
