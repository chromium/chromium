// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Contains method needed to access the forms and their elements.
 */

import {getRemoteFrameToken} from '//components/autofill/ios/form_util/resources/fill_util.js';
import {getFormIdentifier} from '//components/autofill/ios/form_util/resources/form_utils.js';
import {gCrWeb, gCrWebLegacy} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage, trim} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * A WeakMap to track if the current value of a field was entered by user or
 * programmatically.
 * If the map is null, the source of changed is not track.
 */
const wasEditedByUser: WeakMap<any, any> = new WeakMap();

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

// LINT.IfChange(autofill_count_form_submission_in_renderer)
// The source that triggered the sending of the form submission report.
enum FormSubmissionReportSource {
  // Report was sent immediately because quota was available.
  INSTANT,
  // Report was sent from the scheduled task.
  SCHEDULED_TASK,
  // Report was sent from unloading the page content.
  UNLOAD_PAGE,
}
// LINT.ThenChange(//components/autofill/ios/form_util/form_activity_tab_helper.mm:autofill_count_form_submission_in_renderer)

/**
 * Represent the number of form submissions split by type.
 */
interface FormSubmissionCountReport {
  // From a submit event.
  htmlEvent: number;
  // Triggered via `form.submit()`.
  programmatic: number;
}

/**
 * Manager of form submission reports. Takes care of throttling form submission
 * reports via quota and schedules batches of aggregated reports.
 */
class FormSubmissionReportManager {
  /**
   * Time period for refreshing the report quota.
   */
  private static readonly QUOTA_REFRESH_PERIOD_MS = 4000;  // 4 seconds

  /**
   * Time period in milliseconds between each form submission count report.
   */
  private static readonly REPORT_PERIOD_MS = 2000;  // 2 seconds

  /**
   * Number of reports allowed by the quota.
   */
  private static readonly QUOTA_SIZE = 2;

  // Maps the message handler to the pending reports to send to that handler.
  private formSubmissionCountReportMap: Map<string, FormSubmissionCountReport> =
      new Map();

  /**
   * Quota of form submission reports that can be sent before using throttling.
   * Reports sent under the quota are sent directly to the browser without
   * the need for scheduling a report which is much faster and reliable.
   */
  private formSubmissionReportQuotaRemaining =
      FormSubmissionReportManager.QUOTA_SIZE;

  constructor() {
    window.addEventListener('unload', () => {
      // Send the submission count report right now as the document is about to
      // be unloaded, meaning that the reporting scheduled task is likely to be
      // cancelled. This doesn't work when the entire tab is closed.
      this.sendFormSubmissionCountReports(
          FormSubmissionReportSource.UNLOAD_PAGE);
    });
  }

  sendReport(isProgrammatic: boolean, handler: string): void {
    if (!autofillFormFeaturesApi.getFunction(
            'isAutofillCountFormSubmissionInRendererEnabled')()) {
      // Do not report anything if the feature is disabled.
      return;
    }

    const scheduleReport = this.formSubmissionCountReportMap.size === 0;

    // Initialize the report if there isn't already one for the `handler`.
    if (!this.formSubmissionCountReportMap.has(handler)) {
      this.formSubmissionCountReportMap.set(
          handler, {htmlEvent: 0, programmatic: 0});
    }

    const report: FormSubmissionCountReport =
        this.formSubmissionCountReportMap.get(handler)!;

    if (isProgrammatic) {
      ++report.programmatic;
    } else {
      ++report.htmlEvent;
    }

    if (this.formSubmissionReportQuotaRemaining > 0) {
      --this.formSubmissionReportQuotaRemaining;
      // Report right away if the quota wasn't reached yet.
      this.sendFormSubmissionCountReports(FormSubmissionReportSource.INSTANT);
      // Reset the quota after a cooldown period.
      setTimeout(
          () => ++this.formSubmissionReportQuotaRemaining,
          FormSubmissionReportManager.QUOTA_REFRESH_PERIOD_MS);
      return;
    }

    if (scheduleReport) {
      // If no quota is available, schedule a report if there isn't already
      // one pending.
      const reportFn = () => this.sendFormSubmissionCountReports(
          FormSubmissionReportSource.SCHEDULED_TASK);
      setTimeout(reportFn, FormSubmissionReportManager.REPORT_PERIOD_MS);
    }
  }

  /**
   * Sends the `formSubmissionCountReport` (if there is) to the browser.
   */
  private sendFormSubmissionCountReports(source: FormSubmissionReportSource):
      void {
    this.formSubmissionCountReportMap.forEach(
        (report: FormSubmissionCountReport, handler: string) => {
          const message = {
            command: 'form.submit.count',
            ...report,
            source,
          };
          sendWebKitMessage(handler, message);
        });

    this.formSubmissionCountReportMap.clear();
  }
}

const gFormSubmissionReportManager = new FormSubmissionReportManager();

/**
 * Returns the field's `name` attribute if not space only; otherwise the
 * field's `id` attribute.
 *
 * The name will be used as a hint to infer the autofill type of the field.
 *
 * It aims to provide the logic in
 *     WebString nameForAutofill() const;
 * in chromium/src/third_party/WebKit/Source/WebKit/chromium/public/
 *  WebFormControlElement.h
 *
 * @param element An element of which the name for Autofill will be returned.
 * @return the name for Autofill.
 */
function getFieldName(element: Element|null): string {
  if (!element) {
    return '';
  }

  if ('name' in element && element.name) {
    const trimmedName = trim(element.name as string);
    if (trimmedName.length > 0) {
      return trimmedName;
    }
  }

  if (element.id) {
    return trim(element.id);
  }

  return '';
}

/**
 * Returns whether the last `input` or `change` event on `element` was
 * triggered by a user action (was "trusted"). Returns true by default if the
 * feature to fix the user edited bit isn't enabled which is the status quo.
 * TODO(crbug.com/40941928): Match Blink's behavior so that only a 'reset' event
 * makes an edited field unedited.
 */
function fieldWasEditedByUser(element: Element) {
  return !autofillFormFeaturesApi.getFunction(
             'isAutofillCorrectUserEditedBitInParsedField')() ||
      (wasEditedByUser.get(element) ?? false);
}

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
    formData: gCrWebLegacy.fill.autofillSubmissionData(form),
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

/**
 * Reports periodically (as needed) the form submission counts that were
 * detected before doing any processing. The count for each type of event is
 * provided (regular or programmatic).
 * @param isProgrammatic True if the source of the form submission is
 *   programmatic (i.e. comes from the prototype override).
 * @param handler Name of the browser handler to send the message with the count
 *   report to.
 */
function reportDetectedFormSubmission(
    isProgrammatic: boolean, handler: string): void {
  // Ignore reporting if there is an error as this isn't a critical function.
  // Reporting form submission shouldn't have a side effect on processing the
  // form submission.
  try {
    gFormSubmissionReportManager.sendReport(isProgrammatic, handler);
  } catch {
    // Ignore.
  }
}



gCrWebLegacy.form = {
  wasEditedByUser,
  getFieldName,
  fieldWasEditedByUser,
  formSubmitted,
  reportFormSubmissionError,
  reportDetectedFormSubmission,
};
