// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export function assertTrue(value: boolean, message?: string): asserts value;
export function assertFalse(value: boolean, message?: string): void;
export function assertGE(
    value1: number, value2: number, message?: string): void;
export function assertGT(
    value1: number, value2: number, message?: string): void;
export function assertEquals(
    expected: any, actual: any, message?: string): void;
export function assertDeepEquals(
    expected: any, actual: any, message?: string): void;
export function assertLE(
    value1: number, value2: number, message?: string): void;
export function assertLT(
    value1: number, value2: number, message?: string): void;
export function assertNotEquals(
    expected: any, actual: any, message?: string): void;
export function assertNotReached(message?: string): void;
export function assertThrows(
    testFunction: () => any, expectedOrConstructor?: (Function|string|RegExp),
    message?: string): void;
export function assertArrayEquals(expected: any[], actual: any[]): void;
