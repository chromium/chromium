// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function checkImage() {
  return imageLoadedPromise(document.images[0]);
}

function imageLoadedPromise(img_element) {
  return new Promise((resolve, reject) => {
    if (img_element.complete && img_element.src) {
      // Treat the image as failed based on its dimension.
      resolve(img_element.naturalHeight > 0);
    } else {
      img_element.addEventListener('load', () => {
        resolve(true);
      });
      img_element.addEventListener('error', () => {
        resolve(false);
      });
    }
  });
}

function imageSrc() {
  sendValueToTest(document.images[0].src);
}

function sendValueToTest(value) {
  window.domAutomationController.send(value);
}
