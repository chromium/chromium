// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://access-code-cast/access_code_cast.js';

import {AddSinkResultCode, CastDiscoveryMethod, PageCallbackRouter} from 'chrome://access-code-cast/access_code_cast.mojom-webui.js';
import {BrowserProxy} from 'chrome://access-code-cast/browser_proxy.js';
import {RouteRequestResultCode} from 'chrome://access-code-cast/route_request_result_code.mojom-webui.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {TestBrowserProxy} from '../test_browser_proxy.js';

class TestAccessCodeCastBrowserProxy extends TestBrowserProxy {
  constructor (addResult, castResult, castCallback) {
    super([
      'addSink',
      'castToSink'
    ]);

    this.addResult = addResult;
    this.castResult = castResult;
    this.castCallback = castCallback;
  }

  /** @override */
  addSink(accessCode, discoveryMethod) {
    this.methodCalled('addSink', {accessCode, discoveryMethod});
    return Promise.resolve({resultCode: this.addResult});
  }

  /** @override */
  castToSink() {
    this.castCallback();
    this.methodCalled('castToSink');
    return Promise.resolve({resultCode: this.castResult});
  }
}

/**
 * Creates a mock test proxy.
 * @return {TestBrowserProxy}
 */
export function createTestProxy(addResult, castResult, castCallback) {
  const callbackRouter = new PageCallbackRouter();
  return {
    callbackRouter,
    callbackRouterRemote: callbackRouter.$.bindNewPipeAndPassRemote(),
    handler: new TestAccessCodeCastBrowserProxy(
      addResult, castResult, castCallback),
    async isQrScanningAvailable() {
      return Promise.resolve(true);
    },
    closeDialog() {},
    isDialog() {
      return true;
    }
  };
}

suite('AccessCodeCastAppTest', () => {
  /** @type {!AccessCodeCastElement} */
  let app;
  let mockProxy;

  setup(async () => {
    PolymerTest.clearBody();

    mockProxy = createTestProxy(
      AddSinkResultCode.OK,
      RouteRequestResultCode.OK,
      () => {}
    );
    BrowserProxy.setInstance(mockProxy);

    app = document.createElement('access-code-cast-app');
    document.body.appendChild(app);

    await waitAfterNextRender();
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
      () => {}
    );
    BrowserProxy.setInstance(testProxy);

    app.setAccessCodeForTest('qwerty');
    app.switchToCodeInput();

    app.addSinkAndCast();
    testProxy.handler.whenCalled('addSink').then(
      ({accessCode, discoveryMethod}) => {
        assertEquals(accessCode, 'qwerty');
        assertEquals(discoveryMethod, CastDiscoveryMethod.INPUT_ACCESS_CODE);
      }
    );
  });

  test('addSinkAndCast sends correct discoveryMethod to the handler', () => {
    const testProxy = createTestProxy(
      AddSinkResultCode.OK,
      RouteRequestResultCode.OK,
      () => {}
    );
    BrowserProxy.setInstance(testProxy);

    app.setAccessCodeForTest('123456');
    app.switchToQrInput();

    app.addSinkAndCast();
    testProxy.handler.whenCalled('addSink').then(
      ({accessCode, discoveryMethod}) => {
        assertEquals(accessCode, '123456');
        assertEquals(discoveryMethod, CastDiscoveryMethod.QR_CODE);
      }
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
      visitedCallback
    );
    BrowserProxy.setInstance(testProxy);

    assertFalse(visited);
    await app.addSinkAndCast();
    assertTrue(visited);
  });

  test('addSinkAndCast does not call castToSink if add is not successful',
    async () => {
      let visited = false;
      app.setAccessCodeForTest('qwerty');
      const visitedCallback = () => {
        visited = true;
      };
      const testProxy = createTestProxy(
        AddSinkResultCode.UNKNOWN_ERROR,
        RouteRequestResultCode.OK,
        visitedCallback
      );
      BrowserProxy.setInstance(testProxy);

      assertFalse(visited);
      await app.addSinkAndCast();
      assertFalse(visited);
    }
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
        let testProxy = createTestProxy(
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
    };

    // Enter does nothing if the access code isn't the right length
    document.dispatchEvent(new KeyboardEvent('keydown', {'key': 'Enter'}));
    await waitAfterNextRender();
    assertFalse(visited);

    app.setAccessCodeForTest('qwerty');
    document.dispatchEvent(new KeyboardEvent('keydown', {'key': 'Enter'}));
    await waitAfterNextRender();
    assertTrue(visited);
  });

  test('submit button disabled during cast attempt', () => {
    app.setAccessCodeForTest('foobar');
    assertFalse(app.$.castButton.disabled);
    let testProxy = createTestProxy(
      AddSinkResultCode.OK,
      RouteRequestResultCode.OK,
      () => {
        assertTrue(app.$.castButton.disabled);
      }
    );
    BrowserProxy.setInstance(testProxy);
    app.addSinkAndCast();
  });

  test('input is refocused after unsuccessful cast attempts', async () => {
    let testProxy = createTestProxy(
      AddSinkResultCode.OK,
      RouteRequestResultCode.UNKNOWN_ERROR,
      () => {
        // Unfocus the code input during execution of addSinkAndCast.
        app.$.castButton.focus();
        assertFalse(app.$.codeInput.focused);
      }
    );
    BrowserProxy.setInstance(testProxy);
    app.setAccessCodeForTest('foobar');
    // Code input must be focused in order for addSinkAndCast to execute.
    app.$.codeInput.focusInput();

    await app.addSinkAndCast();
    assertTrue(app.$.codeInput.focused);
  });
});
