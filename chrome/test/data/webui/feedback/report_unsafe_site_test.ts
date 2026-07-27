// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://feedback/report_unsafe_site/report_unsafe_site_app.js';

import {browserProxyFactory, PageHandlerRemote} from 'chrome://feedback/report_unsafe_site.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ReportUnsafeSiteTest', () => {
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(PageHandlerRemote);
    browserProxyFactory.setInstance({handler});

    handler.setPromiseResolveFor('getTriggeringPageInfo', {
      pageUrl: '',
      screenshotDataUri: '',
    });
  });

  test('ShowUi', async () => {
    const app = document.createElement('report-unsafe-site-app');
    document.body.appendChild(app);

    await handler.whenCalled('getTriggeringPageInfo');
    await microtasksFinished();
    assertEquals(1, handler.getCallCount('showUi'));
  });

  test('ClickCancel', () => {
    const app = document.createElement('report-unsafe-site-app');
    document.body.appendChild(app);

    const cancelButton =
        app.shadowRoot.querySelector<HTMLInputElement>('#cancel-button');
    assertTrue(!!cancelButton);
    cancelButton.click();
    assertEquals(1, handler.getCallCount('closeDialog'));
  });

  test('ClickSend', async () => {
    handler.setPromiseResolveFor('getTriggeringPageInfo', {
      pageUrl: 'example.com',
      screenshotDataUri: '',
    });

    const app = document.createElement('report-unsafe-site-app');
    document.body.appendChild(app);

    await handler.whenCalled('getTriggeringPageInfo');
    await microtasksFinished();
    const sendButton =
        app.shadowRoot.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!sendButton);
    assertFalse(sendButton.disabled);
    sendButton.click();
    await handler.whenCalled('sendReport');
    assertEquals(1, handler.getCallCount('closeDialog'));
  });

  test('SendButtonDisabled', async () => {
    handler.setPromiseResolveFor('getTriggeringPageInfo', {
      pageUrl: '',
      screenshotDataUri: '',
    });
    const app = document.createElement('report-unsafe-site-app');
    document.body.appendChild(app);

    await handler.whenCalled('getTriggeringPageInfo');
    await microtasksFinished();
    const sendButton =
        app.shadowRoot.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!sendButton);
    assertTrue(sendButton.disabled);
  });

  test('SendButtonDisabledWhenUserClicksSend', async () => {
    handler.setPromiseResolveFor('getTriggeringPageInfo', {
      pageUrl: 'example.com',
      screenshotDataUri: '',
    });
    const app = document.createElement('report-unsafe-site-app');
    document.body.appendChild(app);

    await handler.whenCalled('getTriggeringPageInfo');
    await microtasksFinished();
    const sendButton =
        app.shadowRoot.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!sendButton);
    assertFalse(sendButton.disabled);
    sendButton.click();
    await handler.whenCalled('sendReport');
    assertTrue(sendButton.disabled);
    assertEquals(1, handler.getCallCount('closeDialog'));
  });

  test('IncludeScreenshotCheckbox_HasScreenshot', async () => {
    handler.setPromiseResolveFor('getTriggeringPageInfo', {
      pageUrl: 'example.com',
      screenshotDataUri: 'data:image/png;data:image/png;base64,fakescreenshot',
    });
    const app = document.createElement('report-unsafe-site-app');
    document.body.appendChild(app);

    await handler.whenCalled('getTriggeringPageInfo');
    await microtasksFinished();
    const checkbox = app.$.includeScreenshotCheckbox;
    assertTrue(!!checkbox);
    assertFalse(checkbox.disabled);
    assertFalse(isChildVisible(app, '#screenshot-placeholder'));
    assertTrue(isChildVisible(app, '#screenshot-image'));

    // Click the checkbox to un-include the screenshot.
    checkbox.click();
    await eventToPromise('change', checkbox);
    assertFalse(checkbox.checked);
    assertTrue(isChildVisible(app, '#screenshot-placeholder'));
    assertFalse(isChildVisible(app, '#screenshot-image'));

    // Click the checkbox to include the screenshot.
    checkbox.click();
    await eventToPromise('change', checkbox);
    assertTrue(checkbox.checked);
    assertFalse(isChildVisible(app, '#screenshot-placeholder'));
    assertTrue(isChildVisible(app, '#screenshot-image'));
  });

  test('IncludeScreenshotCheckbox_NoScreenshot', async () => {
    handler.setPromiseResolveFor('getTriggeringPageInfo', {
      pageUrl: 'example.com',
      screenshotDataUri: '',
    });
    const app = document.createElement('report-unsafe-site-app');
    document.body.appendChild(app);

    const checkbox = app.$.includeScreenshotCheckbox;
    assertTrue(!!checkbox);
    await handler.whenCalled('getTriggeringPageInfo');
    await microtasksFinished();
    assertTrue(checkbox.disabled);
    assertFalse(checkbox.checked);
    assertTrue(isChildVisible(app, '#screenshot-placeholder'));
    assertFalse(isChildVisible(app, '#screenshot-image'));
  });
});
