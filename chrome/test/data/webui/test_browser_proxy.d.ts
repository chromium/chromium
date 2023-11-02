// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class TestBrowserProxy<T = any> {
  static fromClass<T>(clazz: {new(): T}): T&TestBrowserProxy<T>;
  constructor(methodNames?: Array<keyof T>);
  methodCalled(methodName: keyof T, ...args: any[]): any;
  whenCalled(methodName: keyof T): Promise<any>;
  resetResolver(methodName: keyof T): void;
  reset(): void;
  getCallCount(methodName: keyof T): number;
  getArgs(methodName: keyof T): any[];
  setResultMapperFor(methodName: keyof T, resultMapper: Function): void;
  setResultFor(methodName: keyof T, value: any): void;
}
