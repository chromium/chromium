// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    function rejectsFakeBlob() {
      const invalidRequest = formatPrintJobRequest(
          'id', 'test job', new ArrayBuffer(0), minimal_ticket);
      invalidRequest.job.document = new (function Blob() {})();
      chrome.test.assertThrows(
          () => chrome.printing.submitJob(invalidRequest, () => {}),
          /Argument must be a Blob\./);
      chrome.test.succeed();
    },

    function rejectsNonPdfBlob() {
      const request = formatPrintJobRequest(
          'id', 'test job',
          new TextEncoder().encode('This is not a PDF document.').buffer,
          minimal_ticket);
      chrome.printing.submitJob(
          request, chrome.test.callbackFail('Invalid document', response => {
            chrome.test.assertEq(undefined, response);
          }));
    },

    function submitsPdf() {
      const url =
          'http://localhost:' + config.testServer.port + '/pdf/test.pdf';
      submitJob(
          'id', 'test job', url, minimal_ticket,
          chrome.test.callbackPass(response => {
            chrome.test.assertNe(undefined, response);
            chrome.test.assertNe(undefined, response.status);
            chrome.test.assertEq(
                chrome.printing.SubmitJobStatus.OK, response.status);
            chrome.test.assertNe(undefined, response.jobId);
          }));
    },
  ]);
});
