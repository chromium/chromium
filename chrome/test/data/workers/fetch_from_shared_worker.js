// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.onconnect = async e => {
  const port = e.ports[0];
  port.onmessage = async e => {
    try {
      const response = await fetch(e.data.url);
      if (!response.ok) {
        port.postMessage(`Bad response: ${response.statusText}`);
        return;
      }
      const text = await response.text();
      port.postMessage(text);
    } catch (error) {
      port.postMessage(`${error}`);
    }
  }
};
