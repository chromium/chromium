// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Adds listeners that are used to handle forms, enabling autofill
 * and the replacement method to dismiss the keyboard needed because of the
 * Autofill keyboard accessory.
 */

import {processChildFrameMessage} from '//components/autofill/ios/form_util/resources/child_frame_registration_lib.js';
import {type FormControlElement, HAS_BEEN_PASSWORD_SYMBOL} from '//components/autofill/ios/form_util/resources/fill_constants.js';
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
 * The MutationObserver tracking password type changes.
 */
let passwordTypeObserver: MutationObserver|null = null;

/**
 * Timer handle for the throttled mutation processing.
 */
let mutationProcessTimeout: ReturnType<typeof setTimeout>|null = null;

/**
 * Strong references to tracked elements, held temporarily while mutation
 * processing is debounced.
 *
 * Because `__gCrElementMap` only stores `WeakRef`s, an element removed from the
 * DOM could be garbage-collected before the debounce timer fires, which would
 * cause its removal to be missed. Holding strong references here keeps detached
 * elements alive until `processFormMutationsOptimized()` runs.
 */
let pendingBatchElements: Element[] = [];

/**
 * Number of `__gCrElementMap` entries already scanned during the current
 * debounce window.
 *
 * `__gCrElementMap` preserves insertion order and new entries are appended,
 * so subsequent callbacks only need to scan entries after this count.
 * Dead `WeakRef` entries are skipped rather than added, so this count can
 * exceed `pendingBatchElements.length`.
 */
let scannedEntryCount = 0;

/**
 * Whether `hasAddedFormControl()` detected an added form control in this
 * debounce window. Used to skip mutation traversal once a control was
 * added.
 */
let formControlAdded = false;

/**
 * Whether each mutation target had descendant form controls when first seen in
 * this debounce window.
 */
let targetHasFormControl = new WeakMap<Node, boolean>();

/**
 * Mutation targets without descendant form controls when first seen, held
 * weakly so detached targets can be garbage-collected before the window ends.
 * Separate from `targetHasFormControl` because `WeakMap` cannot be iterated.
 */
let emptyTargets: Array<WeakRef<ParentNode&Node>> = [];

/**
 * The set of tag names that are considered form elements.
 */
const FORM_TAGS = new Set(['FORM', 'INPUT', 'SELECT', 'OPTION', 'TEXTAREA']);

/**
 * Tag names tracked as form controls by the optimized mutation observer.
 *
 * Excludes `'OPTION'` because `<option>` is only a child of `<select>`
 * (already tracked) and large dropdown lists shouldn't trigger form changes.
 */
const FORM_CONTROL_TAGS = ['FORM', 'INPUT', 'SELECT', 'TEXTAREA'];

/**
 * Selector matching the elements in `FORM_CONTROL_TAGS`.
 */
const FORM_CONTROL_SELECTOR = FORM_CONTROL_TAGS.join(',');

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
 * Returns true if element1 and element2 have an ancestor/descendant
 * (parent-child) relationship directly or indirectly.
 */
function areElementsRelated(
    element1: Element|null, element2: Element|null): boolean {
  if (!element1 || !element2) {
    return false;
  }
  return element1.contains(element2) || element2.contains(element1);
}

/**
 * Returns true if the password fields tracking feature is enabled.
 */
function isTrackPasswordFieldsEnabled(): boolean {
  return autofillFormFeaturesApi.getFunction(
      'isAutofillTrackPasswordFieldsEnabled')();
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
  if (!FORM_TAGS.has(target.tagName) && !fillUtil.isContentEditable(target)) {
    const path = evt.composedPath() as Element[];
    let foundValidTagName = false;

    // Checks if a valid tag name is found in the event path when the tag name
    // of the event target is not valid itself.
    if (path) {
      for (const htmlElement of path) {
        if (FORM_TAGS.has(htmlElement.tagName) ||
            fillUtil.isContentEditable(htmlElement)) {
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

  const isTargetEditable = fillUtil.isContentEditable(target);
  if (evt.target !== lastFocusedElement &&
      (!isTargetEditable || !areElementsRelated(lastFocusedElement, target))) {
    return;
  }
  const form =
      target.tagName === 'FORM' ? target : (target as HTMLFormElement)['form'];
  const field = target.tagName === 'FORM' ? null : target;

  const formRendererID = fillUtil.getUniqueID(form);
  const fieldRendererID = fillUtil.getUniqueID(field);

  let fieldType = 'type' in target ? target.type : '';
  if (!fieldType && isTargetEditable) {
    fieldType = 'contenteditable';
  }
  const fieldValue = 'value' in target ?
      target.value :
      (isTargetEditable ? (target.textContent ?? '') : '');

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
 * Forwards messages to the Child Frame Registration lib.
 */
function processInboundMessage(event: MessageEvent<any>): void {
  processChildFrameMessage(event);
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
    }
  }
  let messagesToSend: object[];
  if (isAutofillOptimizationFormSearchEnabled()) {
    messagesToSend = [];
    if (removedFormMessage) {
      messagesToSend.push(removedFormMessage);
    }
    if (addedFormMessage) {
      messagesToSend.push(addedFormMessage);
    }
  } else {
    messagesToSend =
        [removedFormMessage, addedFormMessage].filter(v => !!v).map(v => v!);
  }
  if (messagesToSend.length > 0 &&
      !sendFormMutationMessagesAfterDelay(messagesToSend, delay, true)) {
    // Count the messages that couldn't be scheduled as dropped.
    formMsgBatchMetadata.dropCount += messagesToSend.length;
  }
}

/**
 * Returns true if `node` has a descendant matching `FORM_CONTROL_SELECTOR`.
 * Calls the prototype directly to avoid DOM clobbering.
 */
function queryHasFormControl(node: Node): boolean {
  const querySelector = node === document ? Document.prototype.querySelector :
                                            Element.prototype.querySelector;
  return querySelector.call(node, FORM_CONTROL_SELECTOR) !== null;
}

/**
 * Returns whether a connected node added by `mutations` is or contains a form
 * control. Targets cached as empty are deferred to `hasControlInEmptyTarget()`.
 */
function hasAddedFormControl(mutations: MutationRecord[]): boolean {
  // Iterates backwards because updates typically append after removals;
  // returning early avoids allocating `addedNodes`.
  for (let m = mutations.length - 1; m >= 0; m--) {
    const mutation = mutations[m]!;
    const target = mutation.target as ParentNode & Node;
    if (!target.isConnected) {
      continue;
    }

    // Caches whether each target has form controls; targets without any skip
    // their `addedNodes` and are rechecked at the end of the debounce window.
    let hasFormControl = targetHasFormControl.get(target);
    if (hasFormControl === undefined) {
      // Mutations are already applied, so an empty target gained no form
      // controls in this batch.
      hasFormControl = queryHasFormControl(target);
      targetHasFormControl.set(target, hasFormControl);
      if (!hasFormControl) {
        emptyTargets.push(new WeakRef(target));
      }
    }
    if (!hasFormControl) {
      continue;
    }

    // Checks whether an added node is or contains a form control.
    for (const node of mutation.addedNodes) {
      if (node.nodeType !== Node.ELEMENT_NODE || !node.isConnected) {
        continue;
      }
      const element = node as Element;
      if (FORM_CONTROL_TAGS.includes(element.tagName) ||
          queryHasFormControl(element)) {
        return true;
      }
    }
  }
  return false;
}

/**
 * Returns true if any target in `emptyTargets` now contains a form control
 * (meaning one was added later in the debounce window).
 */
function hasControlInEmptyTarget(): boolean {
  for (const ref of emptyTargets) {
    const target = ref.deref();
    if (target?.isConnected && queryHasFormControl(target)) {
      return true;
    }
  }
  return false;
}

/**
 * Resets the state collected during a debounce window.
 */
function resetDebounceWindowState(): void {
  pendingBatchElements = [];
  scannedEntryCount = 0;
  formControlAdded = false;
  targetHasFormControl = new WeakMap();
  emptyTargets = [];
}

/**
 * Processes form mutations using the diffing strategy.
 *
 * WebKit creates a JavaScript wrapper for each DOM node accessed from script,
 * so this strategy minimizes node access to reduce allocation/deallocation
 * churn in a tight loop. It never iterates over `mutation.removedNodes`,
 * avoiding subtree crawls and wrapper materialization for removed elements,
 * and limits reads of `mutation.addedNodes` (see `hasAddedFormControl()`).
 *
 * For removals: Instead of parsing removed nodes, we iterate over the active
 * form controls tracked in `document.__gCrElementMap` and verify if they are
 * still connected to the DOM via the lightweight `.isConnected` property.
 *
 * For additions: each observer callback in the debounce window runs
 * `hasAddedFormControl()` on its `addedNodes` until a form control is found,
 * and `hasControlInEmptyTarget()` rechecks targets skipped as empty. If any are
 * found, sends a single generic `form_changed` message.
 */
function processFormMutationsOptimized(): void {
  const removedForms = new Map<Element, string>();
  const removedFieldIDs: string[] = [];
  // Forms owning removed controls that haven't been seen in `removedForms` yet.
  const formsPendingCheck = new Set<HTMLFormElement>();
  let needsFormChanged = formControlAdded;

  // Process form and formless field removals
  // Check if any tracked elements in the global elements map have
  // been disconnected from the DOM.
  const elementMap = getElementMap()!;
  for (const [id, ref] of elementMap) {
    const element = ref.deref() as Element | null;
    if (!element) {
      // Element was garbage-collected before this mutation batch started
      // (tracked elements are held strongly while a batch is pending). Clean
      // up the map entry without reporting removal.
      elementMap.delete(id);
      continue;
    }

    if (element.isConnected) {
      continue;
    }

    const idStr = id.toString();
    const tagName = element.tagName;
    if (tagName === 'FORM') {
      removedForms.set(element, idStr);
    } else if (FORM_CONTROL_TAGS.includes(tagName)) {
      const form = (element as FormControlElement).form;

      // Unowned form controls.
      if (!form) {
        if (isAutofillableElement(element)) {
          // Reports removed unowned autofillable fields.
          removedFieldIDs.push(idStr);
        } else {
          // Falls back to `form_changed` for non-autofillable controls not
          // covered by `form.removal` (e.g. an `<input>` whose type changed to
          // "hidden").
          needsFormChanged = true;
        }
      } else if (!needsFormChanged && !removedForms.has(form)) {
        // The control's form may appear later in `elementMap`, so defer
        // checking until after the loop.
        formsPendingCheck.add(form);
      }
    }
    // Delete the detached element from our active tracking map.
    elementMap.delete(id);
  }

  if (!needsFormChanged) {
    for (const form of formsPendingCheck) {
      if (!removedForms.has(form)) {
        needsFormChanged = true;
        break;
      }
    }
  }

  let removedFormMessage: object|null = null;
  let addedFormMessage: object|null = null;

  if (removedForms.size > 0 || removedFieldIDs.length > 0) {
    removedFormMessage = {
      'command': 'form.removal',
      'frameID': gCrWeb.getFrameId(),
      'removedFormIDs': fillUtil.stringify([...removedForms.values()]),
      'removedFieldIDs': fillUtil.stringify(removedFieldIDs),
    };
  }

  // Reports `form_changed` when the forms may have changed beyond what
  // `form.removal` covers:
  // - `needsFormChanged`: an observer callback saw an added form control, or a
  //   removed control isn't covered by `form.removal`.
  // - `hasControlInEmptyTarget()`: a later callback added a control to a target
  //   cached as empty by an earlier one, so its `addedNodes` were skipped.
  //   Checked last since it queries the DOM.
  if (needsFormChanged || hasControlInEmptyTarget()) {
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

  // Send the messages
  const messagesToSend: object[] = [];
  if (removedFormMessage) {
    messagesToSend.push(removedFormMessage);
  }
  if (addedFormMessage) {
    messagesToSend.push(addedFormMessage);
  }
  // The delay has already been applied in `trackFormMutations`.
  if (messagesToSend.length > 0 &&
      !sendFormMutationMessagesAfterDelay(messagesToSend, 0, true)) {
    formMsgBatchMetadata.dropCount += messagesToSend.length;
  }
}

/**
 * Retains the elements registered since the last call, so that they can't be
 * garbage-collected before the pending batch is processed.
 */
function retainNewTrackedElements(): void {
  const elementMap = getElementMap();
  if (!elementMap) {
    return;
  }
  if (elementMap.size < scannedEntryCount) {
    // Entries were deleted outside of the batch processing, so the scanned
    // entries can't be told apart anymore. Rescans them all; retaining an
    // element twice is harmless.
    scannedEntryCount = 0;
  }
  if (elementMap.size === scannedEntryCount) {
    return;
  }
  let index = 0;
  elementMap.forEach(ref => {
    if (index++ < scannedEntryCount) {
      return;
    }
    const element = ref.deref() as Element | undefined;
    if (element) {
      pendingBatchElements.push(element);
    }
  });
  scannedEntryCount = elementMap.size;
}

/**
 * Initializes the password field type observer.
 */
function initializePasswordFieldTypeObserver(): void {
  passwordTypeObserver = new MutationObserver(function(mutations) {
    for (const mutation of mutations) {
      if (mutation.type === 'attributes') {
        const target = mutation.target as HTMLInputElement;
        if (target.type === 'password') {
          (target as PasswordTrackedElement)[HAS_BEEN_PASSWORD_SYMBOL] = true;
        }
      }
    }
  });
  passwordTypeObserver.observe(document, {
    attributes: true,
    attributeFilter: ['type'],
    subtree: true,
  });
}

/**
 * Clean up the properties used to track form mutations.
 */
function cleanUpFormMutationTracking(): void {
  if (formMutationObserver) {
    formMutationObserver.disconnect();
    formMutationObserver = null;
  }
  if (passwordTypeObserver) {
    passwordTypeObserver.disconnect();
    passwordTypeObserver = null;
  }
  if (mutationProcessTimeout) {
    clearTimeout(mutationProcessTimeout);
    mutationProcessTimeout = null;
  }
  resetDebounceWindowState();
  lastFocusedElement = null;
}

/**
 * Starts observing form mutations.
 * @param observer The MutationObserver to observe for mutations.
 */
function observeFormMutations(observer: MutationObserver|null): void {
  if (!observer) {
    return;
  }
  observer.observe(document, {childList: true, subtree: true});
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
  cleanUpFormMutationTracking();

  if (!delay) {
    return;
  }

  // Track password field mutations.
  if (isTrackPasswordFieldsEnabled()) {
    initializePasswordFieldTypeObserver();
  }

  // Track form mutations.
  if (isAutofillTrackFormMutationsOptimizationEnabled()) {
    formMutationObserver = new MutationObserver(function(mutations) {
      // Keep observing during the delay so that elements registered meanwhile
      // are also retained until the batch is processed.
      retainNewTrackedElements();
      if (!formControlAdded) {
        formControlAdded = hasAddedFormControl(mutations);
      }
      if (mutationProcessTimeout) {
        return;
      }
      mutationProcessTimeout = setTimeout(function() {
        mutationProcessTimeout = null;
        try {
          processFormMutationsOptimized();
        } finally {
          // Released only after processing, so that the removed elements can't
          // be garbage-collected (e.g. by an allocation in the processing)
          // before their removal is detected.
          resetDebounceWindowState();
        }
      }, delay);
    });
    observeFormMutations(formMutationObserver);
  } else {
    formMutationObserver = new MutationObserver(function(mutations) {
      processFormMutationsStandard(mutations, delay);
    });
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
