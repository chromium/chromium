// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {PageCallbackRouter} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

// Test version of the BrowserProxy used in connecting Reading Mode to the color
// pipeline. The color pipeline is called in the connectedCallback when creating
// the app and creates mojo pipelines which we don't need to test here.
export class TestColorUpdaterBrowserProxy extends TestBrowserProxy implements
    BrowserProxy {
  callbackRouter: PageCallbackRouter = new PageCallbackRouter();
}
