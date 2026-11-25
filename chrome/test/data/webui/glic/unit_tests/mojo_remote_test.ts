// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebClientHandlerCallbackRouter, WebClientHandlerRemote} from '//webui-test/glic/glic.mojom-webui.js';
import {wrapMojoRemote} from '//webui-test/glic/glic_api_impl/mojo_remote.js';
import type {MojoRemoteLike} from '//webui-test/glic/glic_api_impl/mojo_remote.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {sleep} from './test_helpers.js';

class FakeMojoRemote implements MojoRemoteLike {
  listeners: Array<() => void> = [];
  calls: string[] = [];
  pendingResolvers: Array<{
    resolve: (v: unknown) => void,
    reject: (e: unknown) => void,
  }> = [];

  onConnectionError = {
    addListener: (listener: () => void) => {
      this.listeners.push(listener);
    },
  };

  triggerConnectionError(reason = 'Message pipe closed.') {
    for (const {reject} of this.pendingResolvers.splice(0)) {
      reject(new Error(reason));
    }
    for (const listener of this.listeners) {
      listener();
    }
  }

  asyncMethod(arg: string): Promise<string> {
    this.calls.push(`asyncMethod:${arg}`);
    const {promise, resolve, reject} = Promise.withResolvers<unknown>();
    this.pendingResolvers.push({resolve, reject});
    return promise as Promise<string>;
  }

  voidMethod(arg: string): void {
    this.calls.push(`voidMethod:${arg}`);
  }

  rejectingMethod(err: unknown): Promise<void> {
    this.calls.push('rejectingMethod');
    return Promise.reject(err);
  }
}

suite('MojoRemoteTest', () => {
  test('forwards normal calls and non-disconnect rejections', async () => {
    const raw = new FakeMojoRemote();
    const wrapped = wrapMojoRemote(raw, {hangOnDisconnect: true});

    const pending = wrapped.asyncMethod('hello');
    assertEquals(1, raw.calls.length);
    raw.pendingResolvers[0]!.resolve('world');
    assertEquals('world', await pending);

    wrapped.voidMethod('ping');
    assertEquals(2, raw.calls.length);

    let caught: unknown;
    try {
      await wrapped.rejectingMethod({appError: 'bad_arg'});
    } catch (e) {
      caught = e;
    }
    assertEquals('bad_arg', (caught as {appError: string}).appError);
  });

  test(
      'hangs in-flight and subsequent calls after onConnectionError',
      async () => {
        const raw = new FakeMojoRemote();
        const wrapped = wrapMojoRemote(raw, {hangOnDisconnect: true});

        let settled = false;
        const inFlight = wrapped.asyncMethod('in_flight')
                             .then(
                                 () => {
                                   settled = true;
                                 },
                                 () => {
                                   settled = true;
                                 });
        void inFlight;

        raw.triggerConnectionError();
        await sleep(0);
        assertFalse(settled);

        let postDisconnectSettled = false;
        const postDisconnect = wrapped.asyncMethod('after').then(
            () => {
              postDisconnectSettled = true;
            },
            () => {
              postDisconnectSettled = true;
            });
        void postDisconnect;

        wrapped.voidMethod('after_void');
        await sleep(0);

        assertFalse(postDisconnectSettled);
        // Calls after disconnect should not be forwarded to the underlying
        // remote.
        assertEquals(1, raw.calls.length);
      });

  test('hangOnDisconnect: false preserves disconnect rejections', async () => {
    const raw = new FakeMojoRemote();
    const wrapped = wrapMojoRemote(raw, {hangOnDisconnect: false});

    let caught: unknown;
    const pending = wrapped.asyncMethod('in_flight').catch((e) => {
      caught = e;
    });
    raw.triggerConnectionError('Message pipe closed.');
    await pending;
    assertTrue(caught instanceof Error);
    assertEquals('Message pipe closed.', caught.message);
  });

  test('real WebClientHandlerRemote hangs on closed pipe', async () => {
    const unhandledRejections: unknown[] = [];
    const listener = (event: PromiseRejectionEvent) => {
      event.preventDefault();
      unhandledRejections.push(event.reason);
    };
    window.addEventListener('unhandledrejection', listener);
    try {
      let settled = false;
      // Bound remote whose peer closes: in-flight and subsequent calls
      // hang.
      const boundRemote = wrapMojoRemote(
          new WebClientHandlerRemote(), {hangOnDisconnect: true});
      const router = new WebClientHandlerCallbackRouter();
      router.$.bindHandle(boundRemote.$.bindNewPipeAndPassReceiver().handle);
      void boundRemote.clearClientStorage().then(
          () => {
            settled = true;
          },
          () => {
            settled = true;
          });
      const disconnected = new Promise<void>(
          (resolve) => boundRemote.onConnectionError.addListener(resolve));
      router.$.close();
      await disconnected;

      void boundRemote.setClientStorageItem('k2', 'v2')
          .then(
              () => {
                settled = true;
              },
              () => {
                settled = true;
              });
      boundRemote.onModeChange(0);

      await sleep(0);
      assertFalse(settled);
      assertEquals(0, unhandledRejections.length);
    } finally {
      window.removeEventListener('unhandledrejection', listener);
    }
  });
});
