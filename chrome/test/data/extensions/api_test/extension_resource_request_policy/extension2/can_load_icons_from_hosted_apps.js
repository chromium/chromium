// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let image = document.createElement('img');
image.onload = () => { chrome.test.notifyPass(); };
image.onerror = () => { chrome.test.notifyFail('Image should have loaded'); };
image.src = 'chrome-extension://ggmldgjhdenlnjjjmehkomheglpmijnf/test.png';
document.body.appendChild(image);
