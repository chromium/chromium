// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This function adds onBeforeRequest listeners to the contained webview
// and navigates to the test file supplied, which must be in the test
// app's directory. It sends |readyMsg| after the listeners are installed,
// which signal the C++ side to continue the test. It sends |successMsg| on
// success, and |failureMsg| on failure. Failure occurs if an unexpected URL
// is encountered in the listener callback.
//
// The function must be called from multiple webviews, each within its
// own iframe, and all the parameters should be unique for each webview.
function runWebRequestTest(htmlFile, readyMsg, successMsg, failureMsg) {
  chrome.test.getConfig(function(config) {
    let url = 'http://localhost:' + config.testServer.port +
        '/extensions/platform_apps/web_view/two_iframes_web_request/' +
        htmlFile;
    let webview = document.querySelector('webview');

    let promises = [];

    // Add a generous numbers of listeners, since we want to ensure
    // the callback is only for the url for the webview. Prior to the
    // fix, the subevents for different webviews would have the same
    // names, which meant that each webview would get the events for
    // all of the webviews.
    for (let i = 0; i < 10; ++i) {
      promises.push(new Promise((resolve, reject) => {
        webview.request.onBeforeRequest.addListener(function(details) {
          if (details.url != url) {
            reject(`Unexpected URL: "${details.url}"`);
          } else {
            resolve();
          }
        }, {urls: [ '<all_urls>' ]});
      }));
    }

    // Send the ready message and update the webview in the
    // callback.
    chrome.test.sendMessage(readyMsg, (message) => {
      webview.src = url;

      Promise.all(promises)
          .then(() => {
            chrome.test.sendMessage(successMsg);
          })
          .catch((error) => {
            console.log(error);
            chrome.test.sendMessage(failureMsg);
          });
    });
  });
}
