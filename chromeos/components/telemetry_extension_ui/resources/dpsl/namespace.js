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
var dpsl = {};
dpsl.telemetry = null;
dpsl.diagnostics = null;
dpsl.system_events = null;
dpsl.internal = {};
dpsl.internal.messagePipe =
  new MessagePipe('chrome://telemetry-extension', window.parent);

/**
 * Namespace for ChromeOS APIs.
 */
var chromeos = {};
chromeos.diagnostics = null;
chromeos.telemetry = null;
