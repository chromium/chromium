// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of InSessionAuth for testing.
 */

import {RequestTokenReply} from 'chrome://resources/mojo/chromeos/components/in_session_auth/mojom/in_session_auth.mojom-webui.js';

/**
 * Fake implementation of TimeDelta used inside the InSessionAuth token reply
 * timeout.
 */
class TimeDelta {
  microseconds: bigint;

  constructor(microseconds: bigint) {
    this.microseconds = microseconds;
  }
}

/**
 * Fake implementation of InSessionAuthInterface
 */
export class FakeInSessionAuth {
  tokenReply_: RequestTokenReply|null;

  constructor() {
    this.tokenReply_ = { token: 'token', timeout: new TimeDelta(BigInt(1000)) }
  }

  requestToken(): {reply: null|RequestTokenReply} {
    return {reply: this.tokenReply_};
  }

  checkToken(): boolean {
    return true;
  }

  invalidateToken() {
    this.tokenReply_ = null;
  }
}
