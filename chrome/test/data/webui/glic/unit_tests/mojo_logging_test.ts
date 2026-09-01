// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {maybeWrapWithLogging, toDebugJson} from '//webui-test/glic/glic_api_impl/mojo_logging.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('MojoLoggingTest', () => {
  let infoLogs: Array<{message: string, payload: unknown}> = [];
  let warnLogs: Array<{message: string, payload: unknown}> = [];

  const originalConsoleInfo = console.info;
  const originalConsoleWarn = console.warn;

  setup(() => {
    infoLogs = [];
    warnLogs = [];
    console.info = (message?: unknown, ...optionalParams: unknown[]) => {
      infoLogs.push({
        message: String(message),
        payload: optionalParams[0],
      });
    };
    console.warn = (message?: unknown, ...optionalParams: unknown[]) => {
      warnLogs.push({
        message: String(message),
        payload: optionalParams[0],
      });
    };
  });

  teardown(() => {
    console.info = originalConsoleInfo;
    console.warn = originalConsoleWarn;
  });

  test(
      'returns target directly when enabled is false or unset in guest load time data',
      () => {
        const target = {
          doSomething(arg: number) {
            return arg * 2;
          },
        };

        const wrapped = maybeWrapWithLogging(target);
        assertEquals(target, wrapped);

        const result = wrapped.doSomething(5);
        assertEquals(10, result);
        assertEquals(0, infoLogs.length);
      });

  test(
      'enables proxy automatically when guest load time data has loggingEnabled: true',
      () => {
        (window as unknown as {
          glicGuestLoadTimeData: {loggingEnabled: boolean},
        }).glicGuestLoadTimeData = {
          loggingEnabled: true,
        };
        try {
          const target = {
            doSomething(arg: number) {
              return arg * 2;
            },
          };

          const wrapped = maybeWrapWithLogging(target);
          assertFalse(target === wrapped);
          const result = wrapped.doSomething(5);
          assertEquals(10, result);
          assertEquals(1, infoLogs.length);
        } finally {
          delete (window as unknown as {
            glicGuestLoadTimeData?: unknown,
          }).glicGuestLoadTimeData;
        }
      });

  test('logs async requests and responses when enabled is true', async () => {
    const target = {
      createTab(url: string) {
        return Promise.resolve({tabId: 42, url});
      },
    };

    const wrapped = maybeWrapWithLogging(target, {
      enabled: true,
      prefix: 'TestRemote',
    });

    assertFalse(target === wrapped);

    const response = await wrapped.createTab('https://example.com');
    assertEquals(42, response.tabId);

    assertEquals(2, infoLogs.length);
    assertTrue(
        infoLogs[0]!.message.includes(
            'TestRemote [createTab] sending request: ["https://example.com"]'),
    );
    assertTrue(
        infoLogs[1]!.message.includes(
            'TestRemote [createTab] received response: {"tabId":42,"url":"https://example.com"}'),
    );
  });

  test('logs async errors when promise rejects', async () => {
    const target = {
      failingMethod() {
        return Promise.reject(new Error('boom'));
      },
    };

    const wrapped = maybeWrapWithLogging(target, {
      enabled: true,
      prefix: 'TestRemote',
    });

    let threw = false;
    try {
      await wrapped.failingMethod();
    } catch (e) {
      threw = true;
    }
    assertTrue(threw);

    assertEquals(1, infoLogs.length);
    assertTrue(
        infoLogs[0]!.message.includes(
            'TestRemote [failingMethod] sending request'),
    );

    assertEquals(1, warnLogs.length);
    assertTrue(
        warnLogs[0]!.message.includes(
            'TestRemote [failingMethod] received error:'),
    );
  });

  test('ignores specified methods like checkResponsive', async () => {
    const target = {
      checkResponsive() {
        return Promise.resolve('pong');
      },
      otherMethod() {
        return Promise.resolve('ok');
      },
    };

    const wrapped = maybeWrapWithLogging(target, {
      enabled: true,
      prefix: 'TestRemote',
    });

    await wrapped.checkResponsive();
    assertEquals(0, infoLogs.length);

    await wrapped.otherMethod();
    assertEquals(2, infoLogs.length);
  });

  test('does not intercept Mojo internal properties', () => {
    const mockMojoEndpoint = {
      close() {},
    };
    const target = {
      $: mockMojoEndpoint,
      regularMethod() {
        return 1;
      },
    };

    const wrapped = maybeWrapWithLogging(target, {enabled: true});
    assertEquals(mockMojoEndpoint, wrapped.$);
  });

  test('toDebugJson serializes BigInt, ArrayBuffer, and TypedArrays', () => {
    const data = {
      bigIntValue: BigInt(123456789012345),
      buffer: new ArrayBuffer(16),
      uint8Array: new Uint8Array(8),
      nested: {
        str: 'hello',
      },
    };

    const json = toDebugJson(data);
    assertEquals(
        '{"bigIntValue":"123456789012345","buffer":"ArrayBuffer(16)","uint8Array":"Uint8Array(8)","nested":{"str":"hello"}}',
        json,
    );
  });

  test('toDebugJson handles circular structures without throwing', () => {
    const circularObj: {name: string, self?: unknown} = {name: 'test'};
    circularObj.self = circularObj;

    const json = toDebugJson(circularObj);
    assertEquals('{"name":"test","self":"[Circular]"}', json);
  });

  test('toDebugJson serializes Map and Set', () => {
    const data = {
      mySet: new Set([1, 'alpha', 2]),
      myMap: new Map([
        ['key1', 'val1'],
        ['key2', 'val2'],
      ]),
    };

    const json = toDebugJson(data);
    assertEquals(
        '{"mySet":[1,"alpha",2],"myMap":{"key1":"val1","key2":"val2"}}',
        json,
    );
  });

  test(
      'toDebugJson formats arbitrary class instances by constructor name',
      () => {
        class CustomClass {
          secret = 'hidden';
        }
        const data = {
          instance: new CustomClass(),
        };

        const json = toDebugJson(data);
        assertEquals('{"instance":"[CustomClass]"}', json);
      });

  test('toDebugJson truncates long arrays and sets with ...', () => {
    const longArray = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12];
    const longSet = new Set([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]);

    const json = toDebugJson({longArray, longSet});
    assertEquals(
        '{"longArray":[0,1,2,3,4,5,6,7,8,9,"..."],"longSet":[0,1,2,3,4,5,6,7,8,9,"..."]}',
        json,
    );
  });
});
