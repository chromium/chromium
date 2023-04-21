// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Adds listeners that are used to handle forms, enabling autofill
 * and the replacement method to dismiss the keyboard needed because of the
 * Autofill keyboard accessory.
 */

// Requires functions from fill.js and form.js.

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected. String 'formHandlers' is used in |__gCrWeb['formHandlers']| as it
 * needs to be accessed in Objective-C code.
 */
__gCrWeb.formHandlers = {};

/**
 * The MutationObserver tracking form related changes.
 */
let formMutationObserver = null;

/**
 * The form mutation message scheduled to be sent to browser.
 */
let formMutationMessageToSend = null;

/**
 * A message scheduled to be sent to host on the next runloop.
 */
let messageToSend = null;

/**
 * The last HTML element that had focus.
 */
let lastFocusedElement = null;

/**
 * The original implementation of HTMLFormElement.submit that will be called by
 * the hook.
 * @private
 */
let formSubmitOriginalFunction = null;

/**
 * Schedule |mesg| to be sent on next runloop.
 * If called multiple times on the same runloop, only the last message is really
 * sent.
 */
function sendMessageOnNextLoop_(mesg) {
  if (!messageToSend) {
    setTimeout(function() {
      __gCrWeb.common.sendWebKitMessage('FormHandlersMessage', messageToSend);
      messageToSend = null;
    }, 0);
  }
  messageToSend = mesg;
}

/** @private
 * @param {string} originalURL A string containing a URL (absolute, relative...)
 * @return {string} A string containing a full URL (absolute with scheme)
 */
function getFullyQualifiedUrl_(originalURL) {
  // A dummy anchor (never added to the document) is used to obtain the
  // fully-qualified URL of |originalURL|.
  const anchor = document.createElement('a');
  anchor.href = originalURL;
  return anchor.href;
}

/**
 * @param {Element} A form element to check.
 * @return {boolean} Whether the element is an input of type password.
 */
function isPasswordField_(element) {
  return element.tagName === 'INPUT' && element.type === 'password';
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
 * @private
 */
function formActivity_(evt) {
  const validTagNames = ['FORM', 'INPUT', 'OPTION', 'SELECT', 'TEXTAREA'];
  let target = evt.target;

  if (!validTagNames.includes(target.tagName)) {
    const path = evt.composedPath();
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
      __gCrWeb.form.wasEditedByUser !== null) {
    __gCrWeb.form.wasEditedByUser.set(target, evt.isTrusted);
  }

  if (evt.target !== lastFocusedElement) {
    return;
  }
  const form = target.tagName === 'FORM' ? target : target.form;
  const field = target.tagName === 'FORM' ? null : target;

  __gCrWeb.fill.setUniqueIDIfNeeded(form);
  const formUniqueId = __gCrWeb.fill.getUniqueID(form);
  __gCrWeb.fill.setUniqueIDIfNeeded(field);
  const fieldUniqueId = __gCrWeb.fill.getUniqueID(field);

  const fieldType = target.type || '';
  const fieldValue = target.value || '';

  const msg = {
    'command': 'form.activity',
    'frameID': __gCrWeb.message.getFrameId(),
    'formName': __gCrWeb.form.getFormIdentifier(form),
    'uniqueFormID': formUniqueId,
    'fieldIdentifier': __gCrWeb.form.getFieldIdentifier(field),
    'uniqueFieldID': fieldUniqueId,
    'fieldType': fieldType,
    'type': evt.type,
    'value': fieldValue,
    'hasUserGesture': evt.isTrusted,
  };
  sendMessageOnNextLoop_(msg);
}


/**
 * Capture form submit actions.
 */
function submitHandler_(evt) {
  if (evt['defaultPrevented']) return;
  if (evt.target.tagName !== 'FORM') {
    return;
  }

  formSubmitted_(evt.target);
}

// Send the form data to the browser.
function formSubmitted_(form) {
  // Default action is to re-submit to same page.
  const action = form.getAttribute('action') || document.location.href;
  __gCrWeb.common.sendWebKitMessage('FormHandlersMessage', {
    'command': 'form.submit',
    'frameID': __gCrWeb.message.getFrameId(),
    'formName': __gCrWeb.form.getFormIdentifier(form),
    'href': getFullyQualifiedUrl_(action),
    'formData': __gCrWeb.fill.autofillSubmissionData(form),
  });
}

/**
 * Schedules |msg| to be sent after |delay|. Until |msg| is sent, further calls
 * to this function are ignored.
 */
function sendFormMutationMessageAfterDelay_(msg, delay) {
  if (formMutationMessageToSend) return;

  formMutationMessageToSend = msg;
  setTimeout(function() {
    __gCrWeb.common.sendWebKitMessage(
        'FormHandlersMessage', formMutationMessageToSend);
    formMutationMessageToSend = null;
  }, delay);
}

function attachListeners_() {
  /**
   * Focus events performed on the 'capture' phase otherwise they are often
   * not received.
   * Input and change performed on the 'capture' phase as they are needed to
   * detect if the current value is entered by the user.
   */
  document.addEventListener('focus', formActivity_, true);
  document.addEventListener('blur', formActivity_, true);
  document.addEventListener('change', formActivity_, true);
  document.addEventListener('input', formActivity_, true);
  document.addEventListener('reset', formActivity_, true);

  /**
   * Other events are watched at the bubbling phase as this seems adequate in
   * practice and it is less obtrusive to page scripts than capture phase.
   */
  document.addEventListener('keyup', formActivity_, false);
  document.addEventListener('submit', submitHandler_, false);

  // Per specification, SubmitEvent is not triggered when calling form.submit().
  // Hook the method to call the handler in that case.
  if (formSubmitOriginalFunction === null) {
    formSubmitOriginalFunction = HTMLFormElement.prototype.submit;
    HTMLFormElement.prototype.submit = function() {
      // If an error happens in formSubmitted_, this will cancel the form
      // submission which can lead to usability issue for the user.
      // Put the formSubmitted_ in a try catch to ensure the original function
      // is always called.
      try {
        formSubmitted_(this);
      } catch (e) {
      }
      formSubmitOriginalFunction.call(this);
    };
  }
}

// Attach the listeners immediately to try to catch early actions of the user.
attachListeners_();

// Initial page loading can remove the listeners. Schedule a reattach after page
// build.
setTimeout(attachListeners_, 1000);

/**
 * Finds recursively all the form control elements in a node list.
 *
 * @param {NodeList} The node list from which to extract the elements.
 * @return {Element} The extracted elements or an empty list if there is no
 *     match.
 */
function findAllFormElementsInNodes_(nodeList) {
  return [...nodeList]
      .filter(n => n.nodeType === Node.ELEMENT_NODE)
      .map(n => [n, ...n.getElementsByTagName('*')])
      .map(
          elems => elems.filter(
              e => e.tagName.match(/^(FORM|INPUT|SELECT|OPTION|TEXTAREA)$/)))
      .flat();
}

/**
 * Finds a password form element, which is defined as a form with
 * at least one password element as the immediate child (depth = 1).
 *
 * For example: <from><input type="password"></form> is considered as a password
 * form.
 *
 * @param {Array<Element>} Array of elements within which to search.
 * @return {HTMLFormElement} Extracted password form or undefined if there is no
 *   match.
 */
function findPasswordForm_(elements) {
  return elements.filter(e => e.tagName === 'FORM')
      .find(e => [...e.elements].some(isPasswordField_));
}

/**
 * Finds the renderer IDs of the formless password input elements in an array of
 * elements.
 *
 * @param {Array<Element>} Array of elements within which to search.
 * @return {Array<String>} Renderer ids of the formless password fields.
 */
function findFormlessPasswordFieldsIds_(elements) {
  return elements
      .filter(e => e.tagName === 'INPUT' && !e.form && isPasswordField_(e))
      .map(__gCrWeb.fill.getUniqueID);
}

/**
 * Installs a MutationObserver to track form related changes. Waits |delay|
 * milliseconds before sending a message to browser. A delay is used because
 * form mutations are likely to come in batches. An undefined or zero value for
 * |delay| would stop the MutationObserver, if any.
 */
__gCrWeb.formHandlers['trackFormMutations'] = function(delay) {
  if (formMutationObserver) {
    formMutationObserver.disconnect();
    formMutationObserver = null;
  }

  if (!delay) return;

  formMutationObserver = new MutationObserver(function(mutations) {
    for (let i = 0; i < mutations.length; i++) {
      const mutation = mutations[i];
      // Only process mutations to the tree of nodes.
      if (mutation.type !== 'childList') {
        continue;
      }

      // Handle added nodes.
      if (findAllFormElementsInNodes_(mutation.addedNodes).length > 0) {
        const msg = {
          'command': 'form.activity',
          'frameID': __gCrWeb.message.getFrameId(),
          'formName': '',
          'uniqueFormID': '',
          'fieldIdentifier': '',
          'uniqueFieldID': '',
          'fieldType': '',
          'type': 'form_changed',
          'value': '',
          'hasUserGesture': false,
        };
        return sendFormMutationMessageAfterDelay_(msg, delay);
      }

      // Handle removed nodes by starting from the specific removal cases down
      // to the generic form modification case.

      const removedFormElements =
          findAllFormElementsInNodes_(mutation.removedNodes);
      const pwdFormGone = findPasswordForm_(removedFormElements);
      if (pwdFormGone) {
        // Handle the removed password form case.
        const uniqueFormId = __gCrWeb.fill.getUniqueID(pwdFormGone);
        const msg = {
          'command': 'pwdform.removal',
          'frameID': __gCrWeb.message.getFrameId(),
          'formName': __gCrWeb.form.getFormIdentifier(pwdFormGone),
          'uniqueFormID': uniqueFormId,
          'uniqueFieldID': '',
        };
        return sendFormMutationMessageAfterDelay_(msg, delay);
      }

      const removedFormlessPasswordFieldsIds =
          findFormlessPasswordFieldsIds_(removedFormElements);
      if (removedFormlessPasswordFieldsIds.length > 0) {
        // Handle the removed formless password field case.
        const msg = {
          'command': 'pwdform.removal',
          'frameID': __gCrWeb.message.getFrameId(),
          'formName': '',
          'uniqueFormID': '',
          'uniqueFieldID': __gCrWeb.stringify(removedFormlessPasswordFieldsIds),
        };
        return sendFormMutationMessageAfterDelay_(msg, delay);
      }

      if (removedFormElements.length > 0) {
        // Handle the removed form control element case as a form changed
        // mutation that is treated the same way as adding a new form.
        const msg = {
          'command': 'form.activity',
          'frameID': __gCrWeb.message.getFrameId(),
          'formName': '',
          'uniqueFormID': '',
          'fieldIdentifier': '',
          'uniqueFieldID': '',
          'fieldType': '',
          'type': 'form_changed',
          'value': '',
          'hasUserGesture': false,
        };
        return sendFormMutationMessageAfterDelay_(msg, delay);
      }
    }
  });
  formMutationObserver.observe(document, {childList: true, subtree: true});
};

/**
 * Enables or disables the tracking of input event sources.
 */
__gCrWeb.formHandlers['toggleTrackingUserEditedFields'] = function(track) {
  if (track) {
    __gCrWeb.form.wasEditedByUser =
        __gCrWeb.form.wasEditedByUser || new WeakMap();
  } else {
    __gCrWeb.form.wasEditedByUser = null;
  }
};
