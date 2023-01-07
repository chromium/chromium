// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testFileSystemURLNavigations() {
    // Construct an initial iframe with basic content to perform the
    // filesystem: URL navigation inside of.
    let starter_iframe = 'iframe.html';
    var iframe = document.createElement('iframe');
    iframe.src = starter_iframe;
    document.body.appendChild(iframe);

    // Constructing the filesystem: URL.
    webkitRequestFileSystem(
      window.TEMPORARY,
      1024,
      function(fs) {
        fs.root.getFile(
          'test.html',
          {create: true, exclusive: false},
          fileEntry => {
            let filesystem_url = fileEntry.toURL();
            chrome.test.assertEq(0, filesystem_url.indexOf('filesystem:'));
            // Once the iframe loads, ensure that it navigated to the
            // filesystem: URL.
            iframe.addEventListener("load", e => {
              if(iframe.src === filesystem_url) {
                chrome.test.assertNoLastError();
                chrome.test.succeed();
              }
            });
            // Navigate from basic content to the filesystem_url.
            iframe.src = filesystem_url;
          });
      }); // end webkitRequestFileSystem()
  } // end testFileSystemURLNavigations()
]);
