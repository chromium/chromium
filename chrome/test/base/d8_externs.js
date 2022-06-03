// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview
 * Externs for d8, v8's shell at v8/src/d8/.
 * @externs
 */

/** @param {...string} var_args */
function print(var_args) {}

/** @param {number} code */
function quit(code) {}

/**
 * @param {string} path
 * @return {string}
 */
function read(path) {}
