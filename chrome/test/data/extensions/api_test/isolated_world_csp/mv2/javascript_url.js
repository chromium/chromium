// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const button = document.createElement('button');
button.id = 'my-button';

// DOM Property event handlers execute in the world they are registered in.
button.onclick = () => {
  chrome.test.notifyPass();
};
document.body.appendChild(button);

location.href = 'javascript:document.getElementById("my-button").click();';
