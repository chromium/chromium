// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Message definitions passed over the HelpApp privileged/unprivileged pipe.
 */

/**
 * Enum for message types.
 * @enum {string}
 */
const Message = {
  OPEN_FEEDBACK_DIALOG: 'open-feedback-dialog',
  SHOW_PARENTAL_CONTROLS: 'show-parental-controls'
};
