// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let video = document.createElement('video');
video.oncanplay = () => { chrome.test.notifyPass(); }
video.src = chrome.runtime.getURL('bear.webm');
document.body.appendChild(video);
