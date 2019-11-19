// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Adds listeners that are used to handle forms, enabling autofill
 * and the replacement method to dismiss the keyboard needed because of the
 * Autofill keyboard accessory.
 */

goog.provide('__crWeb.formHandlers');

goog.require('__crWeb.fill');
goog.require('__crWeb.form');

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected. String 'formHandlers' is used in |__gCrWeb['formHandlers']| as it
 * needs to be accessed in Objective-C code.
 */
__gCrWeb.formHandlers = {};

/** Beginning of anonymous object */
(function() {
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
      __gCrWeb.message.invokeOnHost(messageToSend);
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
 * Focus, input, change, keyup and blur events for form elements (form and input
 * elements) are messaged to the main application for broadcast to
 * WebStateObservers.
 * Events will be included in a message to be sent in a future runloop (without
 * delay). If an event is already scheduled to be sent, it is replaced by |evt|.
 * Notably, 'blur' event will not be transmitted to the main application if they
 * are triggered by the focus of another element as the 'focus' event will
 * replace it.
 * Only the events targeting the active element (or the previously active in
 * case of 'blur') are sent to the main application.
 * This is done with a single event handler for each type being added to the
 * main document element which checks the source element of the event; this
 * is much easier to manage than adding handlers to individual elements.
 * @private
 */
function formActivity_(evt) {
  const target = evt.target;
  if (!['FORM', 'INPUT', 'OPTION', 'SELECT', 'TEXTAREA'].includes(
          target.tagName)) {
    return;
  }
  const value = target.value || '';
  const fieldType = target.type || '';
  if (evt.type !== 'blur') {
    lastFocusedElement = document.activeElement;
  }
  if (['change', 'input'].includes(evt.type) &&
      __gCrWeb.form.wasEditedByUser !== null) {
    __gCrWeb.form.wasEditedByUser.set(target, evt.isTrusted);
  }
  if (target != lastFocusedElement) return;
  const msg = {
    'command': 'form.activity',
    'formName': __gCrWeb.form.getFormIdentifier(evt.target.form),
    'fieldIdentifier': __gCrWeb.form.getFieldIdentifier(target),
    'fieldType': fieldType,
    'type': evt.type,
    'value': value,
    'hasUserGesture': evt.isTrusted
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
  __gCrWeb.message.invokeOnHost({
    'command': 'form.submit',
    'formName': __gCrWeb.form.getFormIdentifier(form),
    'href': getFullyQualifiedUrl_(action),
    'formData': __gCrWeb.fill.autofillSubmissionData(form)
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
    __gCrWeb.message.invokeOnHost(formMutationMessageToSend);
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
 * Installs a MutationObserver to track form related changes. Waits |delay|
 * milliseconds before sending a message to browser. A delay is used because
 * form mutations are likely to come in batches. An undefined or zero value for
 * |delay| would stop the MutationObserver, if any.
 * @suppress {checkTypes} Required for for...of loop on mutations.
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
      if (mutation.type != 'childList') continue;
      const addedElements = [];
      for (let j = 0; j < mutation.addedNodes.length; j++) {
        const node = mutation.addedNodes[j];
        // Ignore non-element nodes.
        if (node.nodeType != Node.ELEMENT_NODE) continue;
        addedElements.push(node);
        [].push.apply(
            addedElements, [].slice.call(node.getElementsByTagName('*')));
      }
      const formChanged = addedElements.find(function(element) {
        return element.tagName.match(/(FORM|INPUT|SELECT|OPTION|TEXTAREA)/);
      });
      if (formChanged) {
        const msg = {
          'command': 'form.activity',
          'formName': '',
          'fieldIdentifier': '',
          'fieldType': '',
          'type': 'form_changed',
          'value': '',
          'hasUserGesture': false
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
/** Flush the message queue. */
if (__gCrWeb.message) {
  __gCrWeb.message.invokeQueues();
}

}());  // End of anonymous object
