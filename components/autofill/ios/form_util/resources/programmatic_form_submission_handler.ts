// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview TODO(update this) Adds listeners that are used to handle forms,
 * enabling autofill and the replacement method to dismiss the keyboard needed
 * because of the Autofill keyboard accessory.
 */

// Requires functions from fill.ts, form.ts, and autofill_form_features.ts.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
//
///**
// * The name of the message handler in the browser layer which will process
// * programmatic form submission messages. This corresponds to
// * ProgrammaticFormSubmissionJavaScriptFeature.
// */
const NATIVE_MESSAGE_HANDLER = 'ProgrammaticFormSubmissionHandlerMessage';
//
///**
// * The original implementation of HTMLFormElement.submit that will be called
// by
// * the hook.
// */
const formSubmitOriginalFunction = HTMLFormElement.prototype.submit;

// Per specification, SubmitEvent is not triggered when calling form.submit().
// Hook the method to call the handler in that case.
HTMLFormElement.prototype.submit = function() {
  // If an error happens in formSubmitted, this will cancel the form
  // submission which can lead to usability issue for the user.
  // Put the formSubmitted in a try catch to ensure the original function
  // is always called.
  try {
    gCrWeb.form.formSubmitted(
        this, /* messageHandler= */ NATIVE_MESSAGE_HANDLER,
        /* programmaticSubmission= */ true,
        /* includeRemoteFrameToken= */ true);
  } catch (e) {
  }
  formSubmitOriginalFunction.call(this);
};
