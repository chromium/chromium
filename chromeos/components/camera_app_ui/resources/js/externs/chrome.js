// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   get: function(Array<string>, function(Object)),
 *   set: function(string, ?),
 * }}
 */
chrome.chromeosInfoPrivate;

/** @const */
chrome.events = {};

/**
 * @typedef {{
 *   addListener: function(function(): void),
 *   removeListener: function(function(): void),
 * }}
 */
chrome.events.Event;
