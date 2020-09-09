// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Converts an unguessable token to a string by combining the high and low
 * values as strings with a hashtag as the separator.
 * @param {!mojoBase.mojom.UnguessableToken} token
 * @return {!string}
 */
export function tokenToString(token) {
  return `${token.high.toString()}#${token.low.toString()}`;
}
