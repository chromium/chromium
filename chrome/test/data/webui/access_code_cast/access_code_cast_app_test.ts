// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://access-code-cast/access_code_cast.js';

import type {AccessCodeCastElement} from 'chrome://access-code-cast/access_code_cast.js';
import {AddSinkResultCode, CastDiscoveryMethod} from 'chrome://access-code-cast/access_code_cast.mojom-webui.js';
import {BrowserProxy} from 'chrome://access-code-cast/browser_proxy.js';
import {RouteRequestResultCode} from 'chrome://access-code-cast/route_request_result_code.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {createTestProxy} from './test_access_code_cast_browser_proxy.js';

suite('AccessCodeCastAppTest', () => {
  let app: AccessCodeCastElement;

  setup(async () => {
    const mockProxy = createTestProxy(
        AddSinkResultCode.OK,
        RouteRequestResultCode.OK,
        () => {},
    );
    BrowserProxy.setInstance(mockProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('access-code-cast-app');
    document.body.appendChild(app);
    await waitAfterNextRender(app);
  });

  test('codeInputView is shown and qrInputView is hidded by default', () => {
    assertFalse(app.$.codeInputView.hidden);
    assertTrue(app.$.qrInputView.hidden);
  });

  test('the "switchToQrInput" function switches the view correctly', () => {
    app.switchToQrInput();

    assertTrue(app.$.codeInputView.hidden);
    assertFalse(app.$.qrInputView.hidden);
  });

  test('the "switchToCodeInput" function switches the view correctly', () => {
    // Start on the qr input view and check we are really there
    app.switchToQrInput();
    assertTrue(app.$.codeInputView.hidden);
    assertFalse(app.$.qrInputView.hidden);

    app.switchToCodeInput();

    assertFalse(app.$.codeInputView.hidden);
    assertTrue(app.$.qrInputView.hidden);
  });

  test('addSinkAndCast sends correct accessCode to the handler', () => {
    const testProxy = createTestProxy(
        AddSinkResultCode.OK,
        RouteRequestResultCode.OK,
        () => {},
    );
    BrowserProxy.setInstance(testProxy);

    app.setAccessCodeForTest('qwerty');
    app.switchToCodeInput();

    app.addSinkAndCast();
    testProxy.handler.whenCalled('addSink').then(
        ({accessCode, discoveryMethod}) => {
          assertEquals(accessCode, 'qwerty');
          assertEquals(discoveryMethod, CastDiscoveryMethod.INPUT_ACCESS_CODE);
        },
    );
  });

  test('addSinkAndCast sends correct discoveryMethod to the handler', () => {
    const testProxy = createTestProxy(
        AddSinkResultCode.OK,
        RouteRequestResultCode.OK,
        () => {},
    );
    BrowserProxy.setInstance(testProxy);

    app.setAccessCodeForTest('123456');
    app.switchToQrInput();

    app.addSinkAndCast();
    testProxy.handler.whenCalled('addSink').then(
        ({accessCode, discoveryMethod}) => {
          assertEquals(accessCode, '123456');
          assertEquals(discoveryMethod, CastDiscoveryMethod.QR_CODE);
        },
    );
  });

  test('addSinkAndCast calls castToSink if add is successful', async () => {
    let visited = false;
    app.setAccessCodeForTest('qwerty');
    const visitedCallback = () => {
      visited = true;
    };
    const testProxy = createTestProxy(
        AddSinkResultCode.OK,
        RouteRequestResultCode.OK,
        visitedCallback,
    );
    BrowserProxy.setInstance(testProxy);

    assertFalse(visited);
    await app.addSinkAndCast();
    assertTrue(visited);
  });

  test(
      'addSinkAndCast does not call castToSink if add is not successful',
      async () => {
        let visited = false;
        app.setAccessCodeForTest('qwerty');
        const visitedCallback = () => {
          visited = true;
        };
        const testProxy = createTestProxy(
            AddSinkResultCode.UNKNOWN_ERROR,
            RouteRequestResultCode.OK,
            visitedCallback,
        );
        BrowserProxy.setInstance(testProxy);

        assertFalse(visited);
        await app.addSinkAndCast();
        assertFalse(visited);
      },
  );

  test(
      'addSinkAndCast surfaces errors and hides errors when user starts ' +
          'editing',
      async () => {
        let testProxy = createTestProxy(
            AddSinkResultCode.UNKNOWN_ERROR, RouteRequestResultCode.OK,
            () => {});
        BrowserProxy.setInstance(testProxy);
        app.setAccessCodeForTest('qwerty');

        assertEquals(0, app.$.errorMessage.getMessageCode());
        await app.addSinkAndCast();
        assertEquals(1, app.$.errorMessage.getMessageCode());

        app.setAccessCodeForTest('qwert');
        assertEquals(0, app.$.errorMessage.getMessageCode());

        testProxy = createTestProxy(
            AddSinkResultCode.INVALID_ACCESS_CODE, RouteRequestResultCode.OK,
            () => {});
        BrowserProxy.setInstance(testProxy);

        app.setAccessCodeForTest('qwerty');
        await app.addSinkAndCast();
        assertEquals(2, app.$.errorMessage.getMessageCode());

        app.setAccessCodeForTest('qwert');
        assertEquals(0, app.$.errorMessage.getMessageCode());

        testProxy = createTestProxy(
            AddSinkResultCode.SERVICE_NOT_PRESENT, RouteRequestResultCode.OK,
            () => {});
        BrowserProxy.setInstance(testProxy);

        app.setAccessCodeForTest('qwerty');
        await app.addSinkAndCast();
        assertEquals(3, app.$.errorMessage.getMessageCode());

        app.setAccessCodeForTest('qwert');
        assertEquals(0, app.$.errorMessage.getMessageCode());

        testProxy = createTestProxy(
            AddSinkResultCode.AUTH_ERROR, RouteRequestResultCode.OK, () => {});
        BrowserProxy.setInstance(testProxy);

        app.setAccessCodeForTest('qwerty');
        await app.addSinkAndCast();
        assertEquals(4, app.$.errorMessage.getMessageCode());

        app.setAccessCodeForTest('qwert');
        assertEquals(0, app.$.errorMessage.getMessageCode());

        testProxy = createTestProxy(
            AddSinkResultCode.TOO_MANY_REQUESTS, RouteRequestResultCode.OK,
            () => {});
        BrowserProxy.setInstance(testProxy);

        app.setAccessCodeForTest('qwerty');
        await app.addSinkAndCast();
        assertEquals(5, app.$.errorMessage.getMessageCode());
      });

  test(
      'addSinkAndCast hides errors when user removes all access code',
      async () => {
        const testProxy = createTestProxy(
            AddSinkResultCode.UNKNOWN_ERROR, RouteRequestResultCode.OK,
            () => {});
        BrowserProxy.setInstance(testProxy);
        app.setAccessCodeForTest('qwerty');

        assertEquals(0, app.$.errorMessage.getMessageCode());
        await app.addSinkAndCast();
        assertEquals(1, app.$.errorMessage.getMessageCode());

        app.setAccessCodeForTest('');
        assertEquals(0, app.$.errorMessage.getMessageCode());
      });


  test('enter key press can cast', async () => {
    let visited = false;
    app.setAccessCodeForTest('qwe');
    const realAddSinkAndCast = app.addSinkAndCast;
    app.addSinkAndCast = () => {
      visited = true;
      return Promise.resolve();
    };

    // Enter does nothing if the access code isn't the right length
    document.dispatchEvent(new KeyboardEvent('keydown', {'key': 'Enter'}));
    assertFalse(visited);

    app.setAccessCodeForTest('qwerty');
    document.dispatchEvent(new KeyboardEvent('keydown', {'key': 'Enter'}));
    assertTrue(visited);

    app.addSinkAndCast = realAddSinkAndCast;
  });

  test('submit button disabled during cast attempt', () => {
    app.setAccessCodeForTest('foobar');
    assertFalse(app.$.castButton.disabled);
    const testProxy = createTestProxy(
      AddSinkResultCode.OK, RouteRequestResultCode.OK, () => {
          assertTrue(app.$.castButton.disabled);
        });
    BrowserProxy.setInstance(testProxy);
    app.addSinkAndCast();
  });

  test('input is refocused after unsuccessful cast attempts', async () => {
    const testProxy = createTestProxy(
        AddSinkResultCode.OK, RouteRequestResultCode.UNKNOWN_ERROR, () => {
          // Unfocus the code input during execution of addSinkAndCast.
          app.$.castButton.focus();
          assertFalse(app.$.codeInput.focused);
        });
    BrowserProxy.setInstance(testProxy);
    app.setAccessCodeForTest('foobar');
    // Code input must be focused in order for addSinkAndCast to execute.
    app.$.codeInput.focusInput();

    await app.addSinkAndCast();
    assertTrue(app.$.codeInput.focused);
  });

  // Split up footnote tests to limit number of await statements used in a
  // single test, since this contributes to test flakiness.
  test('managed footnote is correctly created for short times', async () => {
    assertEquals(app.getManagedFootnoteForTest(), undefined);

    await app.createManagedFootnote(3600 /* One hour */);
    assertTrue(app.getManagedFootnoteForTest().includes('1 hour '));

    await app.createManagedFootnote(7200 /* Two hours */);
    assertTrue(app.getManagedFootnoteForTest().includes('2 hours '));

    await app.createManagedFootnote(86400 /* 1 day */);
    assertTrue(app.getManagedFootnoteForTest().includes('1 day '));

    await app.createManagedFootnote(172800 /* 2 days */);
    assertTrue(app.getManagedFootnoteForTest().includes('2 days '));
  });

  test('managed footnote is correctly created for long times', async () => {
    assertEquals(app.getManagedFootnoteForTest(), undefined);

    await app.createManagedFootnote(2764800 /* 32 days */);
    assertTrue(app.getManagedFootnoteForTest().includes('1 month '));

    await app.createManagedFootnote(5529600 /* 64 days */);
    assertTrue(app.getManagedFootnoteForTest().includes('2 months '));

    await app.createManagedFootnote(31540000 /* 1 year */);
    assertTrue(app.getManagedFootnoteForTest().includes('1 year '));

    await app.createManagedFootnote(63080000 /* 2 years */);
    assertTrue(app.getManagedFootnoteForTest().includes('2 years '));
  });
});
