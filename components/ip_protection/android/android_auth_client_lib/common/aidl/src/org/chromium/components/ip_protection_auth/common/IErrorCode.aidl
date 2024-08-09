// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth.common;

/**
 * Error codes used to communicate errors in callback.
 */
interface IErrorCode {
  /**
   * The error is transient in nature. The request might succeed if tried again.
   */
  const int IP_PROTECTION_AUTH_SERVICE_TRANSIENT_ERROR = 0;
  /**
   * The error is permanent in nature. The request is unlikely to succeed if
   * tried again within a short time.
   */
  const int IP_PROTECTION_AUTH_SERVICE_PERSISTENT_ERROR = 1;
}
