// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Adds listeners that are used to handle forms, enabling autofill
 * and the replacement method to dismiss the keyboard needed because of the
 * Autofill keyboard accessory.
 */

import {processChildFrameMessage} from '//components/autofill/ios/form_util/resources/child_frame_registration_lib.js';
import {HAS_BEEN_PASSWORD_SYMBOL} from '//components/autofill/ios/form_util/resources/fill_constants.js';
import {isAutofillableElement} from '//components/autofill/ios/form_util/resources/fill_element_inference_util.js';
import * as fillUtil from '//components/autofill/ios/form_util/resources/fill_util.js';
import {formSubmitted, reportFormSubmissionError, wasEditedByUser} from '//components/autofill/ios/form_util/resources/fill_web_form.js';
import {getFieldIdentifier, getFormIdentifier, reportDetectedFormSubmission} from '//components/autofill/ios/form_util/resources/form_utils.js';
import {getElementMap} from '//components/autofill/ios/form_util/resources/renderer_id.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * The name of the message handler in the browser layer which will process
 * messages sent by this script. This corresponds to
 * FormHandlersJavaScriptFeature.
 */
const NATIVE_MESSAGE_HANDLER = 'FormHandlersMessage';

/**
 * Metadata surrounding the scheduled batch of form messages.
 */
interface FormMsgBatchMetadata {
  // Number of messages that were dropped while messages were already scheduled.
  dropCount: number;
}

/**
 * An HTMLInputElement that can be tracked with a Symbol property to indicate
 * it has been a password field.
 */
interface PasswordTrackedElement extends HTMLInputElement {
  [key: symbol]: boolean;
}

/**
 * The MutationObserver tracking form related changes.
 */
let formMutationObserver: MutationObserver|null = null;

/**
 * Snapshot of the total number of form controls in the document.
 */
let formControlCount: number = -1;

/**
 * Cache the live HTMLCollections.
 * WebCore automatically updates live collections's length when the DOM changes,
 * eliminating new JS HTMLCollection wrapper allocations and binding lookups on
 * subsequent calls.
 */
let formControlCollections: Array<HTMLCollectionOf<Element>> = [];

/**
 * The set of tag names that are considered form elements.
 */
const FORM_TAGS = new Set(['FORM', 'INPUT', 'SELECT', 'OPTION', 'TEXTAREA']);

/**
 * A message scheduled to be sent to host on the next runloop.
 */
let messageToSend: object|null = null;

/**
 * The last HTML element that had focus.
 */
let lastFocusedElement: Element|null = null;

/**
 * The number of messages scheduled to be sent to browser.
 */
let numberOfPendingMessages: number = 0;

/**
 * Object that contains the metadata surrounding the current batch of form
 * messages.
 */
let formMsgBatchMetadata: FormMsgBatchMetadata = {dropCount: 0};

/**
 * Retrieves the registered 'autofill_form_features' CrWebApi
 * instance for use in this file.
 */
const autofillFormFeaturesApi =
  gCrWeb.getRegisteredApi('autofill_form_features');

/**
 * Parses a string to a boolean.
 * @param boolStr The string to parse as a boolean.
 * @returns The boolean value if parsing worked, null otherwise.
 */
function stringAsBool(boolStr: string): boolean | null {
  switch (boolStr) {
    case 'true':
      return true;
    case 'false':
      return false;
    default:
      return null;
  }
}

/**
 * Returns true if form submission events should be listened to in capture mode.
 */
function shouldListenToFormSubmissionEventsInCaptureMode(): boolean {
  // Interpolate the placeholder and parse its string content to the desired
  // boolean value. It is an error to not be able to parse the placeholder.
  return stringAsBool('{{PlaceholderFormSubmissionListenerCapture}}')!;
}

/**
 * Returns true if autofill optimization form search is enabled.
 */
function isAutofillOptimizationFormSearchEnabled(): boolean {
  return (window as any).gCrWebPlaceholderAutofillOptimizationFormSearch;
}

/**
 * Returns true if autofill track form mutations optimization is enabled.
 */
function isAutofillTrackFormMutationsOptimizationEnabled(): boolean {
  return (window as any).gCrWebPlaceholderTrackFormMutationsOptimization;
}

/**
 * Returns true if the password fields tracking feature is enabled.
 */
function isTrackPasswordFieldsEnabled(): boolean {
  return (window as any).gCrWebPlaceholderAutofillTrackPasswordFieldsIos;
}

/**
 * Schedule `mesg` to be sent on next runloop.
 * If called multiple times on the same runloop, only the last message is really
 * sent.
 */
function sendMessageOnNextLoop(mesg: object): void {
  if (!messageToSend) {
    setTimeout(function() {
      sendWebKitMessage(NATIVE_MESSAGE_HANDLER, messageToSend!);
      messageToSend = null;
    }, 0);
  }
  messageToSend = mesg;
}

/**
 * Focus, input, change, keyup, blur and reset events for form elements (form
 * and input elements) are messaged to the main application for broadcast to
 * WebStateObservers.
 * Events will be included in a message to be sent in a future runloop (without
 * delay). If an event is already scheduled to be sent, it is replaced by |evt|.
 * Notably, 'blur' event will not be transmitted to the main application if they
 * are triggered by the focus of another element as the 'focus' event will
 * replace it.
 * Only the events targeting the active element (or the previously active in
 * case of 'blur') are sent to the main application.
 * 'reset' events are sent to the main application only if they are targeting
 * a password form that has user input in it.
 * This is done with a single event handler for each type being added to the
 * main document element which checks the source element of the event; this
 * is much easier to manage than adding handlers to individual elements.
 */
function formActivity(evt: Event): void {
  if (!evt.target) {
    return;
  }

  let target = evt.target as Element;
  if (!FORM_TAGS.has(target.tagName)) {
    const path = evt.composedPath() as Element[];
    let foundValidTagName = false;

    // Checks if a valid tag name is found in the event path when the tag name
    // of the event target is not valid itself.
    if (path) {
      for (const htmlElement of path) {
        if (FORM_TAGS.has(htmlElement.tagName)) {
          target = htmlElement;
          foundValidTagName = true;
          break;
        }
      }
    }
    if (!foundValidTagName) {
      return;
    }
  }
  if (evt.type !== 'blur') {
    lastFocusedElement = document.activeElement;
  }
  if (['change', 'input'].includes(evt.type) && wasEditedByUser !== null) {
    wasEditedByUser.set(target, evt.isTrusted);
  }

  if (evt.target !== lastFocusedElement) {
    return;
  }
  const form =
      target.tagName === 'FORM' ? target : (target as HTMLFormElement)['form'];
  const field = target.tagName === 'FORM' ? null : target;

  const formRendererID = fillUtil.getUniqueID(form);
  const fieldRendererID = fillUtil.getUniqueID(field);

  const fieldType = 'type' in target ? target.type : '';
  const fieldValue = 'value' in target ? target.value : '';

  const msg = {
    'command': 'form.activity',
    'frameID': gCrWeb.getFrameId(),
    'formName': getFormIdentifier(form),
    'formRendererID': formRendererID,
    'fieldIdentifier': getFieldIdentifier(field),
    'fieldRendererID': fieldRendererID,
    'fieldType': fieldType,
    'type': evt.type,
    'value': fieldValue,
    'hasUserGesture': evt.isTrusted,
  };
  sendMessageOnNextLoop(msg);
}


/**
 * Capture form submit actions. Allow default prevented events (when the feature
 * is enabled) since handling form submission doesn't interfere with the web
 * content.
 */
function submitHandler(evt: Event): void {
  const allowDefaultPrevented = autofillFormFeaturesApi.getFunction('isAutofillAllowDefaultPreventedSubmission')();
  // Ignore the submission if it was preventDefault()ed by the content AND
  // `defaultPrevented` isn't allowed as a feature by Autofill.
  if (evt['defaultPrevented'] && !allowDefaultPrevented) {
    return;
  }

  if (!evt.target || (evt.target as Element).tagName !== 'FORM') {
    return;
  }

  formSubmitted(
      evt.target as HTMLFormElement,
      /* messageHandler= */ NATIVE_MESSAGE_HANDLER,
      /* programmaticSubmission= */ false);
}

/**
 * A wrapper around `submitHandler()` that catches and reports errors that
 * happen before calling utility function formSubmitted().
 */
function submitHandlerWithErrorWrapper(evt: Event): void {
  reportDetectedFormSubmission(
      /*isProgrammatic=*/ false, /*handler=*/ NATIVE_MESSAGE_HANDLER);
  try {
    submitHandler(evt);
  } catch (error) {
    if (autofillFormFeaturesApi.getFunction('isAutofillReportFormSubmissionErrorsEnabled')()) {
      reportFormSubmissionError(
          error, /*programmaticSubmission=*/ false,
          /*handler=*/ NATIVE_MESSAGE_HANDLER);
    } else {
      // Just let the error go through if not reported.
      throw error;
    }
  }
}

/**
 * Schedules `messages` to be sent back-to-back after `delay` and with a `delay`
 * between them.
 *
 * For throttling purpose, won't schedule messages if there are already pending
 * messages scheduled to be sent.
 *
 * @param messages Messages to schedule for sending to the browser.
 * @param delay Scheduling delay.
 * @returns True if the messages are scheduled for sending.
 */
function sendFormMutationMessagesAfterDelay(
    messages: object[], delay: number,
    insertMetadata: boolean = false): boolean {
  // Don't schedule these new `messages` if there are already ones scheduled to
  // be sent. This is for throttling.
  if (numberOfPendingMessages > 0) {
    return false;
  }

  messages.forEach((msg, i) => {
    ++numberOfPendingMessages;
    setTimeout(function() {
      --numberOfPendingMessages;
      if (insertMetadata && numberOfPendingMessages === 0) {
        // Add the metadata.
        const size = i + 1;
        msg = {
          ...msg,
          metadata: {
            dropCount: formMsgBatchMetadata.dropCount,
            size: isNaN(size) ? null : size,
          },
        };

        // Reset the metadata for the next batch.
        formMsgBatchMetadata = {dropCount: 0};
      }
      sendWebKitMessage(NATIVE_MESSAGE_HANDLER, msg);
    }, delay * (1 + i));
  });
  return true;
}

/**
 * Checks if cross-frame filling is enabled and, if so, forwards messages to
 * the Child Frame Registration lib.
 */
function processInboundMessage(event: MessageEvent<any>): void {
  if (autofillFormFeaturesApi.getFunction('isAutofillAcrossIframesEnabled')()) {
    processChildFrameMessage(event);
  }
}

function attachListeners(): void {
  /**
   * Focus events performed on the 'capture' phase otherwise they are often
   * not received.
   * Input and change performed on the 'capture' phase as they are needed to
   * detect if the current value is entered by the user.
   */
  document.addEventListener('focus', formActivity, true);
  document.addEventListener('blur', formActivity, true);
  document.addEventListener('change', formActivity, true);
  document.addEventListener('input', formActivity, true);
  document.addEventListener('reset', formActivity, true);

  /**
   * Other events are watched at the bubbling phase as this seems adequate in
   * practice and it is less obtrusive to page scripts than capture phase.
   */
  document.addEventListener('keyup', formActivity, false);
  document.addEventListener(
      'submit', submitHandlerWithErrorWrapper,
      shouldListenToFormSubmissionEventsInCaptureMode());

  /**
   * Receipt of cross-frame messages for Child Frame Registration don't use the
   * `formActivity` handler, but need to be attached under the same conditions.
   */
  window.addEventListener('message', processInboundMessage);
}

/**
 * Scan the page for password fields and set the HAS_BEEN_PASSWORD_SYMBOL on
 * them.
 */
function markPasswordFields(): void {
  const passwordFields = document.querySelectorAll('input[type="password"]');
  for (const passwordField of passwordFields) {
    (passwordField as PasswordTrackedElement)[HAS_BEEN_PASSWORD_SYMBOL] = true;
  }
}

// Attach the listeners immediately to try to catch early actions of the user.
attachListeners();

// Initial page loading can remove the listeners. Schedule a reattach after page
// build. This won't double attach listeners.
setTimeout(attachListeners, 1000);

/**
 * Finds recursively all the form control elements in a node list.
 *
 * @param nodeList The node list from which to extract the elements.
 * @return The extracted elements or an empty list if there is no
 *     match.
 */
function findAllFormElementsInNodes(nodeList: NodeList): Element[] {
  // The feature should give the same result in both case.
  // The only difference should be performance.
  if (isAutofillOptimizationFormSearchEnabled()) {
    const elements: Element[] = [];

    // Filter using a single for-loop instead of array functions
    // to minimize intermediate memory allocations that affect performance.
    for (const node of nodeList) {
      if (node.nodeType !== Node.ELEMENT_NODE) {
        continue;
      }

      const element = node as Element;
      if (FORM_TAGS.has(element.tagName)) {
        elements.push(element);
      }
      const descendants =
          element.querySelectorAll('FORM, INPUT, SELECT, OPTION, TEXTAREA');
      for (const descendant of descendants) {
        elements.push(descendant);
      }
    }
    return elements;
  } else {
    return [...nodeList]
               .filter(n => n.nodeType === Node.ELEMENT_NODE)
               .map(n => [n, ...(n as Element).getElementsByTagName('*')])
               .map(
                   elems => elems.filter(
                       e => (e as Element)
                                .tagName.match(
                                    /^(FORM|INPUT|SELECT|OPTION|TEXTAREA)$/)))
               .flat() as Element[];
  }
}

/**
 * Finds the renderer IDs of the formless input elements in an array of
 * elements.
 *
 * @param elements Array of elements within which to search.
 * @return Renderer ids of the formless fields.
 */
function findFormlessFieldsIds(elements: Element[]): string[] {
  if (isAutofillOptimizationFormSearchEnabled()) {
    const result: string[] = [];
    for (const e of elements) {
      if (isAutofillableElement(e) && !(e as HTMLInputElement).form) {
        result.push(fillUtil.getUniqueID(e));
      }
    }
    return result;
  } else {
    return elements
        .filter(e => isAutofillableElement(e) && !(e as HTMLInputElement).form)
        .map(fillUtil.getUniqueID);
  }
}

/**
 * Processes form mutations using the standard DOM-traversal strategy.
 *
 * @param mutations The list of mutation records from the MutationObserver.
 * @param delay The scheduling delay for sending messages.
 */
function processFormMutationsStandard(
    mutations: MutationRecord[], delay: number): void {
  // Message for the first added form found in the mutations, if there is.
  let addedFormMessage: object|null = null;
  // Message for the first removed form found in the mutations, if there is.
  let removedFormMessage: object|null = null;

  for (const mutation of mutations) {
    // Process mutations to the tree of nodes.
    if (mutation.type === 'childList') {
      const addedFormElements = findAllFormElementsInNodes(mutation.addedNodes);

      // For all password field in the added nodes, set
      // HAS_BEEN_PASSWORD_SYMBOL.
      if (isTrackPasswordFieldsEnabled()) {
        for (const element of addedFormElements) {
          if (element.tagName === 'INPUT' &&
              (element as HTMLInputElement).type === 'password') {
            (element as PasswordTrackedElement)[HAS_BEEN_PASSWORD_SYMBOL] =
                true;
          }
        }
      }

      // Handle added nodes.
      const formWasAdded = addedFormElements.length > 0;
      if (!addedFormMessage && formWasAdded) {
        addedFormMessage = {
          'command': 'form.activity',
          'frameID': gCrWeb.getFrameId(),
          'formName': '',
          'formRendererID': '',
          'fieldIdentifier': '',
          'fieldRendererID': '',
          'fieldType': '',
          'type': 'form_changed',
          'value': '',
          'hasUserGesture': false,
        };
      } else if (formWasAdded) {
        ++formMsgBatchMetadata.dropCount;
      }

      // Handle removed nodes by starting from the specific removal cases down
      // to the generic form modification case.

      const removedFormElements =
          findAllFormElementsInNodes(mutation.removedNodes);

      if (removedFormElements.length === 0) {
        continue;
      }

      const forms = removedFormElements.filter(e => e.tagName === 'FORM');

      const removedFormlessFieldsIds =
          findFormlessFieldsIds(removedFormElements);
      const formlessFieldsWereRemoved = removedFormlessFieldsIds.length > 0;

      // Send removed forms and unowned field id's in the same message.
      if (forms.length > 0 || formlessFieldsWereRemoved) {
        // Drop removed form message if there is one scheduled.
        if (removedFormMessage) {
          ++formMsgBatchMetadata.dropCount;
          continue;
        } else {
          // Send the removed forms identifiers to the browser.
          const filteredFormIDs = forms.map(form => fillUtil.getUniqueID(form));
          removedFormMessage = {
            'command': 'form.removal',
            'frameID': gCrWeb.getFrameId(),
            'removedFormIDs': fillUtil.stringify(filteredFormIDs),
            'removedFieldIDs': fillUtil.stringify(removedFormlessFieldsIds),
          };
          continue;
        }
      }

      if (!removedFormMessage && formlessFieldsWereRemoved) {
        // Handle the removed formless field case.
        removedFormMessage = {
          'command': 'form.removal',
          'frameID': gCrWeb.getFrameId(),
          'removedFieldIDs': fillUtil.stringify(removedFormlessFieldsIds),
        };
        continue;
      } else if (formlessFieldsWereRemoved) {
        ++formMsgBatchMetadata.dropCount;
        continue;
      }

      if (!addedFormMessage) {
        // Handle the removed form control element case as a form changed
        // mutation that is treated the same way as adding a new form.
        addedFormMessage = {
          'command': 'form.activity',
          'frameID': gCrWeb.getFrameId(),
          'formName': '',
          'formRendererID': '',
          'fieldIdentifier': '',
          'fieldRendererID': '',
          'fieldType': '',
          'type': 'form_changed',
          'value': '',
          'hasUserGesture': false,
        };
      } else {
        ++formMsgBatchMetadata.dropCount;
      }
    } else if (
        // Monitors password fields that changes type during its lifetime.
        isTrackPasswordFieldsEnabled() && mutation.type === 'attributes' &&
        mutation.attributeName === 'type') {
      const target = mutation.target as HTMLInputElement;
      if (target.tagName === 'INPUT' && target.type === 'password') {
        (target as PasswordTrackedElement)[HAS_BEEN_PASSWORD_SYMBOL] = true;
      }
    }
  }
  const messagesToSend: object[] =
      [removedFormMessage, addedFormMessage].filter(v => !!v).map(v => v!);
  if (messagesToSend.length > 0 &&
      !sendFormMutationMessagesAfterDelay(messagesToSend, delay, true)) {
    // Count the messages that couldn't be scheduled as dropped.
    formMsgBatchMetadata.dropCount += messagesToSend.length;
  }
}

/**
 * Lazily initializes the live HTMLCollection caches.
 */
function initializeFormControlCollections(): void {
  if (formControlCollections.length === 0) {
    formControlCollections =
        [...FORM_TAGS].map(tag => document.getElementsByTagName(tag));
  }
}

/**
 * Gets the total count of form elements using live HTMLCollections.
 */
function getFormControlCount(): number {
  let total = 0;
  for (const collection of formControlCollections) {
    total += collection.length;
  }
  return total;
}

/**
 * Processes form mutations using the diffing strategy.
 *
 * This strategy avoids iterating over `mutation.removedNodes` and
 * `mutation.addedNodes`, eliminating subtree crawls and preventing JavaScript
 * wrapper materializations for non-form elements. WebView creates a wrapper
 * around the DOM object when we access it, so we want to avoid it to reduce
 * allocation/deallocation churn in a tight loop.
 *
 * For removals: Instead of parsing removed nodes, we iterate over the active
 * form controls tracked in `document.__gCrElementMap` and verify if they are
 * still connected to the DOM via the lightweight `.isConnected` property.
 *
 * For additions: We calculate if any form control elements were added
 * using cached live HTMLCollections. If any additions are detected, send
 * a single generic form activity message.
 *
 * @param mutations The list of mutation records from the MutationObserver.
 * @param delay The scheduling delay for sending messages.
 */
function processFormMutationsOptimized(
    mutations: MutationRecord[], delay: number): void {
  const newFormControlCount = getFormControlCount();

  const removedFormIDs: string[] = [];
  const removedFieldIDs: string[] = [];
  let removedFormControlCount = 0;

  // Process form and formless field removals
  // Check if any tracked elements in the global elements map have
  // been disconnected from the DOM.
  const elementMap = getElementMap()!;
  elementMap.forEach((ref, id) => {
    const element = ref.deref() as Element | null;
    if (!element) {
      // Element was garbage-collected, which implies it was already
      // disconnected from the DOM and is not part of the current mutation
      // records (which hold strong references to removed nodes). We simply
      // clean up our map without reporting it as a new removal. This
      // ensures that future mutation processing iterations are faster.
      elementMap.delete(id);
      return;
    }

    if (element.isConnected) {
      return;
    }

    const idStr = id.toString();
    if (element.tagName === 'FORM') {
      removedFormIDs.push(idStr);
    } else if (
        isAutofillableElement(element) && !(element as HTMLInputElement).form) {
      // Handles unowned fields deletion in formless forms.
      removedFieldIDs.push(idStr);
    }

    // Mathematically align removedFormControlCount with getFormControlCount()
    if (FORM_TAGS.has(element.tagName)) {
      removedFormControlCount++;
    }

    // Delete the detached element from our active tracking map.
    elementMap.delete(id);
  });

  let removedFormMessage: object|null = null;
  let addedFormMessage: object|null = null;

  if (removedFormIDs.length > 0 || removedFieldIDs.length > 0) {
    removedFormMessage = {
      'command': 'form.removal',
      'frameID': gCrWeb.getFrameId(),
      'removedFormIDs': fillUtil.stringify(removedFormIDs),
      'removedFieldIDs': fillUtil.stringify(removedFieldIDs),
    };
  }

  // Process form changed

  // Calculate if any form elements were added to avoid `addedNodes` subtree
  // crawls. The formula is based on:
  // newFormControlCount = formControlCount + addedFormControlCount -
  // removedFormControlCount where:
  //  `newFormControlCount` = form elements count after applying the mutations.
  //  `formControlCount` = form elements count before applying the mutations.
  //  `addedFormControlCount` = number of form elements added in this mutation
  //  `removedFormControlCount` = number of form elements removed in this
  //  mutation.
  const addedFormControlCount =
      (newFormControlCount - formControlCount) + removedFormControlCount;

   // Handle the removed form control element case as a form changed
   // mutation. Removed form control are elements that are not form nor
   // unowned autofillable fields.
  if (addedFormControlCount > 0 ||
      removedFormControlCount >
          (removedFormIDs.length + removedFieldIDs.length)) {
    if (addedFormControlCount > 0 && isTrackPasswordFieldsEnabled()) {
      markPasswordFields();
    }
    addedFormMessage = {
      'command': 'form.activity',
      'frameID': gCrWeb.getFrameId(),
      'formName': '',
      'formRendererID': '',
      'fieldIdentifier': '',
      'fieldRendererID': '',
      'fieldType': '',
      'type': 'form_changed',
      'value': '',
      'hasUserGesture': false,
    };
  }

  // Update the count snapshot
  formControlCount = newFormControlCount;

  // Send the messages
  const messagesToSend: object[] =
      [removedFormMessage, addedFormMessage].filter(v => !!v).map(v => v!);
  if (messagesToSend.length > 0 &&
      !sendFormMutationMessagesAfterDelay(messagesToSend, delay, true)) {
    formMsgBatchMetadata.dropCount += messagesToSend.length;
  }

  // Monitor password fields that change type during their lifetime.
  if (isTrackPasswordFieldsEnabled()) {
    for (let i = 0; i < mutations.length; i++) {
      const m = mutations[i];
      if (m && m.type === 'attributes' && m.attributeName === 'type') {
        const target = m.target as HTMLInputElement;
        if (target.tagName === 'INPUT' && target.type === 'password') {
          (target as PasswordTrackedElement)[HAS_BEEN_PASSWORD_SYMBOL] = true;
        }
      }
    }
  }
}

/**
 * Installs a MutationObserver to track form related changes. Waits |delay|
 * milliseconds before sending a message to browser. A delay is used because
 * form mutations are likely to come in batches. An undefined or zero value for
 * |delay| would stop the MutationObserver, if any. Batches
 * messages for removed and added forms together. This relaxes the messages
 * throttling and allows correctly handling form replacements.
 */
function trackFormMutations(delay: number): void {
  if (formMutationObserver) {
    formMutationObserver.disconnect();
    formMutationObserver = null;
  }

  if (!delay) {
    return;
  }

  if (isTrackPasswordFieldsEnabled()) {
    markPasswordFields();
  }

  if (isAutofillTrackFormMutationsOptimizationEnabled()) {
    initializeFormControlCollections();
    formControlCount = getFormControlCount();
  }

  formMutationObserver = new MutationObserver(function(mutations) {
    if (isAutofillTrackFormMutationsOptimizationEnabled()) {
      processFormMutationsOptimized(mutations, delay);
    } else {
      processFormMutationsStandard(mutations, delay);
    }
  });

  // There is a small performance cost when adding attributes and
  // attributesFilter.
  if (isTrackPasswordFieldsEnabled()) {
    formMutationObserver.observe(document, {
      childList: true,
      subtree: true,
      attributes: true,
      attributeFilter: ['type'],
    });
  } else {
    formMutationObserver.observe(document, {childList: true, subtree: true});
  }
}

const formHandlersApi = new CrWebApi('formHandlers');

formHandlersApi.addFunction('trackFormMutations', trackFormMutations);

try {
  gCrWeb.registerApi(formHandlersApi);
} catch (error) {
  if (error instanceof Error && error.name === 'CrWebError' &&
      error.message === 'API formHandlers already registered.') {
    // TODO(crbug.com/483452015): Refactor this script to stop registering an
    // API in a script which is reinjected with `FeatureScript::
    // ReinjectionBehavior::kReinjectOnDocumentRecreation`.
  } else {
    throw error;
  }
}
