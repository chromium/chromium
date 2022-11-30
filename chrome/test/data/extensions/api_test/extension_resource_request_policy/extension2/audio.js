// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let audio = document.createElement('audio');
audio.oncanplay = () => { chrome.test.notifyPass(); };
audio.src = chrome.runtime.getURL('bear.wav');
document.body.appendChild(audio);
