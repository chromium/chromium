// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function run_tests() {
  // Compute the size of the popup, which will decrease each time.
  var width = 1000;
  var height = 1000;
  if (localStorage.height) {
    height = parseInt(localStorage.height);
  }
  if (localStorage.width) {
    width = parseInt(localStorage.width);
  }

  // Write the new size for next time before generating the resize event.
  var newWidth = width - 500;
  var newHeight = height - 500;
  localStorage.width = JSON.stringify(newWidth);
  localStorage.height = JSON.stringify(newHeight);

  // Set the div's size.
  var test = document.getElementById("test");
  test.style.width = width + "px";
  test.style.height = height + "px";
  chrome.test.log("height: " + test.offsetHeight);
  chrome.test.log("width: " + test.offsetWidth);
}

window.addEventListener("load", function() {
    window.setTimeout(run_tests, 0)
}, false);
