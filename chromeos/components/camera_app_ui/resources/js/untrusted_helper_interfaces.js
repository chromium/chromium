// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   initGA: function(string, string, boolean, function(string): void):
 *     !Promise,
 *   sendGAEvent: function(!ga.Fields): !Promise,
 *   setMetricsEnabled: function(string, boolean): !Promise,
 * }}
 */
export let GAHelperInterface;

/**
 * @typedef {{
 *   connectToWorker: function(!Port): !Promise,
 * }}
 */
export let VideoProcessorHelperInterface;
