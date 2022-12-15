// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onInstalled.addListener(() => {
    var config = {
      mode: "fixed_servers",
      rules: {
        proxyForHttps: {scheme: "http", host: "google.com", port: 5555},
        bypassList: ["127.0.0.1"]
      }
    };
    chrome.proxy.settings.set(
      { 'value': config, 'scope': 'regular' },
      () => {
        chrome.test.sendMessage('ready');
      }
    );
  }
)
