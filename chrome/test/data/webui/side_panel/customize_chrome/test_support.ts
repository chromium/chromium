// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

type Constructor<T> = new (...args: any[]) => T;

export function createMock<T extends object>(clazz: Constructor<T>):
    {mock: T, callTracker: TestBrowserProxy} {
  const callTracker = new TestBrowserProxy(
      Object.getOwnPropertyNames(clazz.prototype)
          .filter(methodName => methodName !== 'constructor'));
  const handler = {
    get: function(_target: T, prop: string) {
      if (clazz.prototype[prop] instanceof Function) {
        return (...args: any[]) => callTracker.methodCalled(prop, ...args);
      }
      if (Object.getOwnPropertyDescriptor(clazz.prototype, prop)!.get) {
        return callTracker.methodCalled(prop);
      }
      return undefined;
    },
  };
  return {mock: new Proxy<T>({} as unknown as T, handler), callTracker};
}

type Installer<T> = (instance: T) => void;

export function installMock<T extends object>(
    clazz: Constructor<T>, installer?: Installer<T>): TestBrowserProxy {
  installer = installer ||
      (clazz as unknown as {setInstance: Installer<T>}).setInstance;
  const {mock, callTracker} = createMock(clazz);
  installer!(mock);
  return callTracker;
}
