// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Interface definitions used throughout the library.
 */

/**
 * Namespace for DPSL APIs.
 */
var chromeos = {};
chromeos.diagnostics = null;
chromeos.telemetry = null;
chromeos.internal = {};
chromeos.internal.messagePipe =
  new MessagePipe('chrome://telemetry-extension', window.parent);

/**
 * This is only for testing purposes. Please don't use it in the production,
 * because we may silently change or remove it.
*/
chromeos.test_support = {};
chromeos.test_support.messagePipe = function () {
  console.warn(
    'messagePipe() is a method for testing purposes only. Please',
    'do not use it, otherwise your app may be broken in the future.');
  return chromeos.internal.messagePipe;
}
