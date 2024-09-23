// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Adds listeners that are used to handle forms, enabling autofill
 * and the replacement method to dismiss the keyboard needed because of the
 * Autofill keyboard accessory.
 */

// Requires functions from fill.ts, form.ts, and autofill_form_features.ts.

import {processChildFrameMessage} from '//components/autofill/ios/form_util/resources/child_frame_registration_lib.js';
import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * Metadata surrounding the scheduled batch of form messages.
 */
interface FormMsgBatchMetadata {
  // Number of messages that were dropped while messages were already scheduled.
  dropCount: number;
}

/**
 * The MutationObserver tracking form related changes.
 */
let formMutationObserver: MutationObserver|null = null;

/**
 * A message scheduled to be sent to host on the next runloop.
 */
let messageToSend: object|null = null;

/**
 * The last HTML element that had focus.
 */
let lastFocusedElement: Element|null = null;

/**
 * The original implementation of HTMLFormElement.submit that will be called by
 * the hook.
 */
let formSubmitOriginalFunction: Function|null = null;

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
 * Schedule `mesg` to be sent on next runloop.
 * If called multiple times on the same runloop, only the last message is really
 * sent.
 */
function sendMessageOnNextLoop(mesg: object): void {
  if (!messageToSend) {
    setTimeout(function() {
      sendWebKitMessage('FormHandlersMessage', messageToSend!);
      messageToSend = null;
    }, 0);
  }
  messageToSend = mesg;
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
  const validTagNames = ['FORM', 'INPUT', 'OPTION', 'SELECT', 'TEXTAREA'];
  if (!evt.target) {
    return;
  }

  let target = evt.target as Element;
  if (!validTagNames.includes(target.tagName)) {
    const path = evt.composedPath() as Element[];
    let foundValidTagName = false;

    // Checks if a valid tag name is found in the event path when the tag name
    // of the event target is not valid itself.
    if (path) {
      for (const htmlElement of path) {
        if (validTagNames.includes(htmlElement.tagName)) {
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
  if (['change', 'input'].includes(evt.type) &&
      gCrWeb.form.wasEditedByUser !== null) {
    gCrWeb.form.wasEditedByUser.set(target, evt.isTrusted);
  }

  if (evt.target !== lastFocusedElement) {
    return;
  }
  const form =
      target.tagName === 'FORM' ? target : (target as HTMLFormElement)['form'];
  const field = target.tagName === 'FORM' ? null : target;

  const formRendererID = gCrWeb.fill.getUniqueID(form);
  const fieldRendererID = gCrWeb.fill.getUniqueID(field);

  const fieldType = 'type' in target ? target.type : '';
  const fieldValue = 'value' in target ? target.value : '';

  const msg = {
    'command': 'form.activity',
    'frameID': gCrWeb.message.getFrameId(),
    'formName': gCrWeb.form.getFormIdentifier(form),
    'formRendererID': formRendererID,
    'fieldIdentifier': gCrWeb.form.getFieldIdentifier(field),
    'fieldRendererID': fieldRendererID,
    'fieldType': fieldType,
    'type': evt.type,
    'value': fieldValue,
    'hasUserGesture': evt.isTrusted,
  };
  sendMessageOnNextLoop(msg);
}


/**
 * Capture form submit actions.
 */
function submitHandler(evt: Event): void {
  if (evt['defaultPrevented']) return;
  if (!evt.target || (evt.target as Element).tagName !== 'FORM') {
    return;
  }

  formSubmitted(evt.target as HTMLFormElement);
}

// Send the form data to the browser.
function formSubmitted(form: HTMLFormElement): void {
  // Default action is to re-submit to same page.
  const action = form.getAttribute('action') || document.location.href;
  sendWebKitMessage('FormHandlersMessage', {
    'command': 'form.submit',
    'frameID': gCrWeb.message.getFrameId(),
    'formName': gCrWeb.form.getFormIdentifier(form),
    'href': getFullyQualifiedUrl(action),
    'formData': gCrWeb.fill.autofillSubmissionData(form),
  });
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
      sendWebKitMessage('FormHandlersMessage', msg);
    }, delay * (1 + i));
  });
  return true;
}

/**
 * Checks if cross-frame filling is enabled and, if so, forwards messages to
 * the Child Frame Registration lib.
 */
function processInboundMessage(event: MessageEvent<any>): void {
  if (gCrWeb.autofill_form_features.isAutofillAcrossIframesEnabled()) {
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
  document.addEventListener('submit', submitHandler, false);

  /**
   * Receipt of cross-frame messages for Child Frame Registration don't use the
   * `formActivity` handler, but need to be attached under the same conditions.
   */
  window.addEventListener('message', processInboundMessage);

  // Per specification, SubmitEvent is not triggered when calling form.submit().
  // Hook the method to call the handler in that case.
  if (formSubmitOriginalFunction === null) {
    formSubmitOriginalFunction = HTMLFormElement.prototype.submit;
    HTMLFormElement.prototype.submit = function() {
      // If an error happens in formSubmitted, this will cancel the form
      // submission which can lead to usability issue for the user.
      // Put the formSubmitted in a try catch to ensure the original function
      // is always called.
      try {
        formSubmitted(this);
      } catch (e) {
      }
      formSubmitOriginalFunction!.call(this);
    };
  }
}

// Attach the listeners immediately to try to catch early actions of the user.
attachListeners();

// Initial page loading can remove the listeners. Schedule a reattach after page
// build.
setTimeout(attachListeners, 1000);

/**
 * Finds recursively all the form control elements in a node list.
 *
 * @param nodeList The node list from which to extract the elements.
 * @return The extracted elements or an empty list if there is no
 *     match.
 */
function findAllFormElementsInNodes(nodeList: NodeList): Element[] {
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

/**
 * Finds the renderer IDs of the formless input elements in an array of
 * elements.
 *
 * @param elements Array of elements within which to search.
 * @return Renderer ids of the formless fields.
 */
function findFormlessFieldsIds(elements: Element[]): string[] {
  return elements
      .filter(
          e => gCrWeb.fill.isAutofillableElement(e) &&
              !(e as HTMLInputElement).form)
      .map(gCrWeb.fill.getUniqueID);
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

  if (!delay) return;

  formMutationObserver = new MutationObserver(function(mutations) {
    // Message for the first added form found in the mutations, if there is.
    let addedFormMessage: object|null = null;
    // Message for the first removed form found in the mutations, if there is.
    let removedFormMessage: object|null = null;

    for (const mutation of mutations) {
      // Only process mutations to the tree of nodes.
      if (mutation.type !== 'childList') {
        continue;
      }

      // Handle added nodes.
      const formWasAdded =
          findAllFormElementsInNodes(mutation.addedNodes).length > 0;
      if (!addedFormMessage && formWasAdded) {
        addedFormMessage = {
          'command': 'form.activity',
          'frameID': gCrWeb.message.getFrameId(),
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
          const filteredFormIDs =
              forms.map(form => gCrWeb.fill.getUniqueID(form));
          removedFormMessage = {
            'command': 'form.removal',
            'frameID': gCrWeb.message.getFrameId(),
            'removedFormIDs': gCrWeb.stringify(filteredFormIDs),
            'removedFieldIDs': gCrWeb.stringify(removedFormlessFieldsIds),
          };
          continue;
        }
      }

      if (!removedFormMessage && formlessFieldsWereRemoved) {
        // Handle the removed formless field case.
        removedFormMessage = {
          'command': 'form.removal',
          'frameID': gCrWeb.message.getFrameId(),
          'removedFieldIDs': gCrWeb.stringify(removedFormlessFieldsIds),
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
          'frameID': gCrWeb.message.getFrameId(),
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
    }
    const messagesToSend: object[] =
        [removedFormMessage, addedFormMessage].filter(v => !!v).map(v => v!);
    if (messagesToSend.length > 0 &&
        !sendFormMutationMessagesAfterDelay(messagesToSend, delay, true)) {
      // Count the messages that couldn't be scheduled as dropped.
      formMsgBatchMetadata.dropCount += messagesToSend.length;
    }
  });
  formMutationObserver.observe(document, {childList: true, subtree: true});
}


/**
 * Enables or disables the tracking of input event sources.
 */
function toggleTrackingUserEditedFields(track: boolean): void {
  if (track) {
    gCrWeb.form.wasEditedByUser = gCrWeb.form.wasEditedByUser || new WeakMap();
  } else {
    gCrWeb.form.wasEditedByUser = null;
  }
}

gCrWeb.formHandlers = {trackFormMutations, toggleTrackingUserEditedFields};
