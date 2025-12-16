// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Contains method needed to access the forms and their elements.
 */

import {getRemoteFrameToken} from '//components/autofill/ios/form_util/resources/fill_util.js';
import {autofillSubmissionData} from '//components/autofill/ios/form_util/resources/fill_web_form.js';
import {getFormIdentifier} from '//components/autofill/ios/form_util/resources/form_utils.js';
import {gCrWeb, gCrWebLegacy} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * Registry that tracks the forms that were submitted during the frame's
 * lifetime. Elements that are garbage collected will be removed from the
 * registry so this can't memory leak. In the worst case the registry will get
 * as big as the number of submitted forms that aren't yet deleted and we don't
 * expect a lot of those.
 */
const formSubmissionRegistry: WeakSet<any> = new WeakSet();

/**
 * Retrieves the registered 'autofill_form_features' CrWebApi
 * instance for use in this file.
 */
const autofillFormFeaturesApi =
  gCrWeb.getRegisteredApi('autofill_form_features');

/**
 * @param originalURL A string containing a URL (absolute, relative...)
 * @return A string containing a full URL (absolute with scheme)
 */
function getFullyQualifiedUrl(originalURL: string): string {
  // A dummy anchor (never added to the document) is used to obtain the
  // fully-qualified URL of `originalURL`.
  const anchor = document.createElement('a');
  anchor.href = originalURL;
  return anchor.href;
}

// Send the form data to the browser.
function formSubmittedInternal(
    form: HTMLFormElement,
    messageHandler: string,
    programmaticSubmission: boolean,
    includeRemoteFrameToken: boolean = false,
    ): void {
  if (autofillFormFeaturesApi.getFunction('isAutofillDedupeFormSubmissionEnabled')()) {
    // Handle deduping when the feature allows it.
    if (formSubmissionRegistry.has(form)) {
      // Do not double submit the same form.
      return;
    }
    formSubmissionRegistry.add(form);
  }

  // Default URL for action is the document's URL.
  const action = form.getAttribute('action') || document.URL;

  const message = {
    command: 'form.submit',
    frameID: gCrWeb.getFrameId(),
    formName: getFormIdentifier(form),
    href: getFullyQualifiedUrl(action),
    formData: autofillSubmissionData(form),
    remoteFrameToken: includeRemoteFrameToken ? getRemoteFrameToken() :
                                                undefined,
    programmaticSubmission: programmaticSubmission,
  };

  sendWebKitMessage(messageHandler, message);
}

/**
 * Sends the form data to the browser. Errors that are caught via the try/catch
 * are reported to the browser. This is done before the error bubbles above
 * `formSubmitted()` so the generic JS errors wrapper doesn't intercept the
 * error before this custom error handler.
 *
 * @param form The form that was submitted.
 * @param messageHandler The name of the message handler to send the message to.
 * @param programmaticSubmission True if the form submission is programmatic.
 * @includeRemoteFrameToken True if the remote frame token should be included
 *   in the payload of the message sent to the browser.
 */
function formSubmitted(
    form: HTMLFormElement,
    messageHandler: string,
    programmaticSubmission: boolean,
    includeRemoteFrameToken: boolean = false,
    ): void {
  try {
    formSubmittedInternal(
        form, messageHandler, programmaticSubmission, includeRemoteFrameToken);
  } catch (error) {
    if (autofillFormFeaturesApi.getFunction('isAutofillReportFormSubmissionErrorsEnabled')()) {
      reportFormSubmissionError(error, programmaticSubmission, messageHandler);
    } else {
      // Just let the error go through if not reported.
      throw error;
    }
  }
}

/**
 * Reports a form submission error to the browser.
 * @param error Object that holds information on the error.
 * @param programmaticSubmission True if the submission that errored was
 *   programmatic.
 * @param handler The name of the handler to send the error message to.
 */
function reportFormSubmissionError(
    error: any, programmaticSubmission: boolean, handler: string) {
  let errorMessage = '';
  let errorStack = '';
  if (error && error instanceof Error) {
    errorMessage = error.message;
    if (error.stack) {
      errorStack = error.stack;
    }
  }

  const message = {
    command: 'form.submit.error',
    errorStack,
    errorMessage,
    programmaticSubmission,
  };
  sendWebKitMessage(handler, message);
}



gCrWebLegacy.form = {
  formSubmitted,
  reportFormSubmissionError,
};
