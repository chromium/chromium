// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class TestBrowserProxy {
  static fromClass<T>(clazz: {new(): T}): T&TestBrowserProxy;
  constructor(methodNames?: Array<string>);
  methodCalled(methodName: string, ...args: any[]): any;
  whenCalled(methodName: string): Promise<any>;
  resetResolver(methodName: string): void;
  reset(): void;
  getCallCount(methodName: string): number;
  getArgs(methodName: string): Array<any>;
  setResultMapperFor(methodName: string, resultMapper: Function): void;
  setResultFor(methodName: string, value: any): void;
}
