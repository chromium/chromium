// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://office-fallback/office_fallback_dialog.js';

import {browserProxyFactory, DialogChoice, PageHandlerRemote} from 'chrome://office-fallback/office_fallback.mojom-webui.js';
import {OfficeFallbackBrowserProxy} from 'chrome://office-fallback/office_fallback_browser_proxy.js';
import type {OfficeFallbackElement} from 'chrome://office-fallback/office_fallback_dialog.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

interface ProxyArgs {
  titleText: string;
  reasonMessage: string;
  instructionsMessage: string;
  enableRetryOption: boolean;
  enableQuickOfficeOption: boolean;
}

/**
 * A test OfficeFallbackBrowserProxy implementation that enables to mock
 * dialog arguments.
 */
class OfficeFallbackTestBrowserProxy implements OfficeFallbackBrowserProxy {
  dialogArgs: string;

  constructor(args: ProxyArgs) {
    // Creating JSON string as in OfficeFallbackDialog::GetDialogArgs().
    this.dialogArgs = JSON.stringify(args);
  }

  // JSON-encoded dialog arguments.
  getDialogArguments(): string {
    return this.dialogArgs;
  }
}

suite('<office-fallback>', () => {
  // Holds the <cloud-upload> app.
  let container: HTMLElement;
  // The <office-fallback> app.
  let officeFallbackApp: OfficeFallbackElement;
  // The mock PageHandlerRemote to make assertions on when mojo methods are
  // called.
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  const setUp =
      (enableRetryOption = true, enableQuickOfficeOption = true,
       reasonMessage = 'a reason') => {
        handler = TestMock.fromClass(PageHandlerRemote);
        browserProxyFactory.setInstance({handler});
        const dialogArgs: ProxyArgs = {
          titleText: 'a title',
          reasonMessage: reasonMessage,
          instructionsMessage: 'an instruction',
          enableRetryOption: enableRetryOption,
          enableQuickOfficeOption: enableQuickOfficeOption,
        };
        const testProxy = new OfficeFallbackTestBrowserProxy(dialogArgs);
        OfficeFallbackBrowserProxy.setInstance(testProxy);

        // Creates and attaches the <office-fallback> element to the DOM tree.
        officeFallbackApp = document.createElement('office-fallback');
        container.appendChild(officeFallbackApp);
      };

  /**
   * Runs prior to all the tests running, attaches a div to enable isolated
   * clearing and attaching of the web component.
   */
  suiteSetup(() => {
    container = document.createElement('div');
    document.body.appendChild(container);
  });


  /**
   * Runs after each test. Removes all elements from the <div> that holds
   * the <cloud-upload> component.
   */
  teardown(() => {
    container.innerHTML = window.trustedTypes!.emptyHTML;
    handler.reset();
  });

  /**
   * Tests that the "try again" and "cancel" buttons are shown when the
   * `enableRetryOption` is enabled.
   */
  test('Try again and cancel buttons shown', () => {
    setUp(true, true);
    assertEquals(officeFallbackApp.$('#try-again-button').style.display, '');
    assertEquals(officeFallbackApp.$('#cancel-button').style.display, '');
    assertEquals(officeFallbackApp.$('#ok-button').style.display, 'none');
  });

  /**
   * Tests that the "OK" button is shown when the `enableRetryOption` is
   * disabled.
   */
  test('OK button shown', () => {
    setUp(false, true);
    assertEquals(
        officeFallbackApp.$('#try-again-button').style.display, 'none');
    assertEquals(officeFallbackApp.$('#cancel-button').style.display, 'none');
    assertEquals(officeFallbackApp.$('#ok-button').style.display, '');
  });

  /**
   * Tests that the "quick office" button is shown when the
   * `enableQuickOfficeOption` is enabled.
   */
  test('Quick office button shown', () => {
    setUp(true, true);
    assertEquals(officeFallbackApp.$('#quick-office-button').style.display, '');
  });

  /**
   * Tests that the "quick office" button is hidden when the
   * `enableQuickOfficeOption` is disabled.
   */
  test('Quick office button hidden', () => {
    setUp(true, false);
    assertEquals(
        officeFallbackApp.$('#quick-office-button').style.display, 'none');
  });

  /**
   * Tests that clicking the "quick office" button triggers the right `close`
   * mojo request.
   */
  test('Open in basic editor button', async () => {
    setUp();

    officeFallbackApp.$('#quick-office-button').click();
    await handler.whenCalled('close');
    assertEquals(1, handler.getCallCount('close'));
    assertDeepEquals([DialogChoice.kQuickOffice], handler.getArgs('close'));
  });

  /**
   * Tests that clicking the "try again" button triggers the right `close`
   * mojo request.
   */
  test('Try again button', async () => {
    setUp();

    officeFallbackApp.$('#try-again-button').click();
    await handler.whenCalled('close');
    assertEquals(1, handler.getCallCount('close'));
    assertDeepEquals([DialogChoice.kTryAgain], handler.getArgs('close'));
  });

  /**
   * Tests that clicking the "Cancel" button triggers the right `close`
   * mojo request.
   */
  test('Cancel button', async () => {
    setUp();

    officeFallbackApp.$('#cancel-button').click();
    await handler.whenCalled('close');
    assertEquals(1, handler.getCallCount('close'));
    assertDeepEquals([DialogChoice.kCancel], handler.getArgs('close'));
  });

  /**
   * Tests that clicking the "OK" button triggers the right `close`
   * mojo request.
   */
  test('OK button', async () => {
    setUp();

    officeFallbackApp.$('#ok-button').click();
    await handler.whenCalled('close');
    assertEquals(1, handler.getCallCount('close'));
    assertDeepEquals([DialogChoice.kOk], handler.getArgs('close'));
  });

  /**
   * Tests that an "escape" keydown triggers the right `close`
   * mojo request.
   */
  test('Escape', async () => {
    setUp();

    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape'}));
    await handler.whenCalled('close');
    assertEquals(1, handler.getCallCount('close'));
    assertDeepEquals([DialogChoice.kCancel], handler.getArgs('close'));
  });
});
