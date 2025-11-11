// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Uuid} from '//resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import {PageCallbackRouter} from 'chrome://contextual-tasks/contextual_tasks.mojom-webui.js';
import type {PageHandlerInterface} from 'chrome://contextual-tasks/contextual_tasks.mojom-webui.js';
import type {BrowserProxy} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * Test version of the ContextualTasksPageHandler used to verify calls to the
 * browser from WebUI.
 */
class TestContextualTasksPageHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  private url_: Url;

  constructor(url: string) {
    super([
      'getThreadUrl',
      'getUrlForTask',
      'setTaskId',
      'setThreadTitle',
      'closeSidePanel',
    ]);

    this.url_ = {url};
  }

  getThreadUrl() {
    this.methodCalled('getThreadUrl');
    return Promise.resolve({url: this.url_});
  }

  getUrlForTask(uuid: Uuid) {
    this.methodCalled('getUrlForTask', uuid);
    return Promise.resolve({url: this.url_});
  }

  setTaskId(uuid: Uuid) {
    this.methodCalled('setTaskId', uuid);
  }

  setThreadTitle(title: string) {
    this.methodCalled('setThreadTitle', title);
  }

  closeSidePanel() {
    this.methodCalled('closeSidePanel');
  }
}

/**
 * Test version of the BrowserProxy used in connecting the Contextual
 * Tasks page to the browser on start up.
 */
export class TestContextualTasksBrowserProxy extends TestBrowserProxy implements
    BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: TestContextualTasksPageHandler;

  /**
   * @param url The URL to load in the iframe.
   */
  constructor(url: string) {
    super([]);
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new TestContextualTasksPageHandler(url);
  }
}
