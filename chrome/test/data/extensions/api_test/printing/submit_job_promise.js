// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(async function(config) {
  const url = `http://localhost:${config.testServer.port}/pdf/test.pdf`;
  let response = await submitJobPromise('id', 'test job', url);
  chrome.test.assertTrue(!!response);
  chrome.test.assertEq(chrome.printing.SubmitJobStatus.OK, response.status);
  chrome.test.assertTrue(!!response.jobId);

  chrome.test.notifyPass();
});
