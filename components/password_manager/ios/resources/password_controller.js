// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Installs Passwords management functions on the __gCrWeb object.
 *
 * It scans the DOM, extracting and storing password forms and returns a JSON
 * string representing an array of objects, each of which represents an Passord
 * form with information about a form to be filled and/or submitted and it can
 * be translated to struct FormData for further processing.
 */

goog.provide('__crWeb.passwords');

/* Beginning of anonymous object. */
(function() {

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected.
 */
__gCrWeb.passwords = {};
__gCrWeb['passwords'] = __gCrWeb.passwords;

/**
 * Finds all password forms in the window and returns form data as a JSON
 * string.
 * @return {string} Form data as a JSON string.
 */
__gCrWeb.passwords['findPasswordForms'] = function() {
  var formDataList = [];
  if (hasPasswordField(window)) {
    getPasswordFormDataList(formDataList, window);
  }
  return __gCrWeb.stringify(formDataList);
};

/** Returns true if the supplied window or any frames inside contain an input
 * field of type 'password'.
 * @private
 * @param {Window} win Whether the supplied window or any frames inside
 * contain an input field of type 'password'.
 * @return {boolean}
 */
var hasPasswordField = function(win) {
  var doc = win.document;

  // We may will not be allowed to read the 'document' property from a frame
  // that is in a different domain.
  if (!doc) {
    return false;
  }

  if (doc.querySelector('input[type=password]')) {
    return true;
  }

  return getSameOriginFrames(win).some(hasPasswordField);
};

/**
 * Returns the contentWindow of all iframes that are from the the same origin
 * as the containing window.
 * @param {Window} win The window in which to look for frames.
 * @return {Array<Window>} Array of the same-origin frames found.
 */
var getSameOriginFrames = function(win) {
  var frames = win.document.getElementsByTagName('iframe');
  var result = [];
  for (var i = 0; i < frames.length; i++) {
    if (!frames[i].src ||
        __gCrWeb.common.isSameOrigin(win.location.href, frames[i].src)) {
      result.push(frames[i].contentWindow);
    }
  }
  return result;
};

/**
 * If |form| has no submit elements and exactly 1 button that button
 * is assumed to be a submit button. This function adds onSubmitButtonClick_
 * as a handler for touchend event of this button. Touchend event is used as
 * a proxy for onclick event because onclick handling might be prevented by
 * the site JavaScript.
 */
var addSubmitButtonTouchEndHandler = function(form) {
  if (form.querySelector('input[type=submit]')) {
    return;
  }
  // Try to find buttons of type submit at first.
  var buttons = form.querySelectorAll('button[type="submit"]');
  if (buttons.length == 0) {
    // Try to check all buttons. If there is only one button, assume that this
    // is the submit button.
    buttons = form.querySelectorAll('button');
    if (buttons.length != 1) {
      return;
    }
  }
  for (var i = 0; i < buttons.length; ++i) {
    buttons[0].addEventListener('touchend', onSubmitButtonTouchEnd);
  }
};

/**
 * Click handler for the submit button. It sends to the host
 * form.submitButtonClick command.
 */
var onSubmitButtonTouchEnd = function(evt) {
  var form = evt.currentTarget.form;
  var formData = __gCrWeb.passwords.getPasswordFormData(form);
  if (!formData) {
    return;
  }
  formData['command'] = 'passwordForm.submitButtonClick';
  __gCrWeb.message.invokeOnHost(formData);
};

/**
 * Returns the element from |inputs| which has the field identifier equal to
 * |identifier| and null if there is no such element.
 * @param {Array<HTMLInputElement>} inputs
 * @param {string} identifier
 * @return {HTMLInputElement}
 */
var findInputByFieldIdentifier = function(inputs, identifier) {
  for (var i = 0; i < inputs.length; ++i) {
    if (identifier == __gCrWeb.form.getFieldIdentifier(inputs[i])) {
      return inputs[i];
    }
  }
  return null;
};

/**
 * Returns the password form with the given |identifier| as a JSON string
 * from the frame |win| and all its same-origin subframes.
 * @param {Window} win The window in which to look for forms.
 * @param {string} identifier The name of the form to extract.
 * @return {HTMLFormElement} The password form.
 */
var getPasswordFormElement = function(win, identifier) {
  var el = win.__gCrWeb.form.getFormElementFromIdentifier(identifier);
  if (el) {
    return el;
  }
  var frames = getSameOriginFrames(win);
  for (var i = 0; i < frames.length; ++i) {
    el = getPasswordFormElement(frames[i], identifier);
    if (el) {
      return el;
    }
  }
  return null;
};

/**
 * Returns an array of input elements in a form.
 * @param {HTMLFormElement} form A form element for which the input elements
 *   are returned.
 * @return {Array<HTMLInputElement>}
 */
var getFormInputElements = function(form) {
  return __gCrWeb.form.getFormControlElements(form).filter(function(element) {
    return element.tagName === 'INPUT';
  });
};

/**
 * Returns the password form with the given |identifier| as a JSON string.
 * @param {string} identifier The identifier of the form to extract.
 * @return {string} The password form.
 */
__gCrWeb.passwords['getPasswordFormDataAsString'] = function(identifier) {
  var el = getPasswordFormElement(window, identifier);
  if (!el) {
    return '{}';
  }
  var formData = __gCrWeb.passwords.getPasswordFormData(el);
  if (!formData) {
    return '{}';
  }
  return __gCrWeb.stringify(formData);
};

/**
 * Finds the form described by |formData| and fills in the
 * username and password values.
 *
 * This is a public function invoked by Chrome. There is no information
 * passed to this function that the page does not have access to anyway.
 *
 * @param {AutofillFormData} formData Form data.
 * @param {string} username The username to fill.
 * @param {string} password The password to fill.
 * @param {string=} opt_normalizedOrigin The origin URL to compare to.
 * @return {boolean} Whether a form field has been filled.
 */
__gCrWeb.passwords['fillPasswordForm'] = function(
    formData, username, password, opt_normalizedOrigin) {
  var normalizedOrigin = opt_normalizedOrigin ||
      __gCrWeb.common.removeQueryAndReferenceFromURL(window.location.href);
  var origin = /** @type {string} */ (formData['origin']);
  if (!__gCrWeb.common.isSameOrigin(origin, normalizedOrigin)) {
    return false;
  }
  return fillPasswordFormWithData(
      formData, username, password, window, opt_normalizedOrigin);
};

/**
 * Fills all password fields in the form identified by |formName|
 * with |password|.
 *
 * @param {string} formName The name of the form to fill.
 * @param {string} newPasswordIdentifier The id of password element to fill.
 * @param {string} confirmPasswordIdentifier The id of confirm password element
 *   to fill.
 * @param {string} password The password to fill.
 * @return {boolean} Whether new password field has been filled.
*/
__gCrWeb.passwords['fillPasswordFormWithGeneratedPassword'] = function(
    formName, newPasswordIdentifier, confirmPasswordIdentifier, password) {
  var form = __gCrWeb.form.getFormElementFromIdentifier(formName);
  if (!form) {
    return false;
  }
  var inputs = getFormInputElements(form);
  var newPasswordField =
      findInputByFieldIdentifier(inputs, newPasswordIdentifier);
  if (!newPasswordField) {
    return false;
  }
  // Avoid resetting if same value, as it moves cursor to the end.
  if (newPasswordField.value != password) {
    __gCrWeb.fill.setInputElementValue(password, newPasswordField);
  }
  var confirmPasswordField =
      findInputByFieldIdentifier(inputs, confirmPasswordIdentifier);
  if (confirmPasswordField && confirmPasswordField.value != password) {
    __gCrWeb.fill.setInputElementValue(password, confirmPasswordField);
  }
  return true;
};

/**
 * Given a description of a form (origin, action and input fields),
 * finds that form on the page and fills in the specified username
 * and password.
 *
 * @param {AutofillFormData} formData Form data.
 * @param {string} username The username to fill.
 * @param {string} password The password to fill.
 * @param {Window} win A window or a frame containing formData.
 * @param {string=} opt_normalizedOrigin The origin URL to compare to.
 * @return {boolean} Whether a form field has been filled.
 */
var fillPasswordFormWithData = function(
    formData, username, password, win, opt_normalizedOrigin) {
  var doc = win.document;
  var forms = doc.forms;
  var filled = false;

  for (var i = 0; i < forms.length; i++) {
    var form = forms[i];
    var normalizedFormAction =
        opt_normalizedOrigin || __gCrWeb.fill.getCanonicalActionForForm(form);
    if (formData.action != normalizedFormAction) {
      continue;
    }
    var inputs = getFormInputElements(form);
    var usernameInput =
        findInputByFieldIdentifier(inputs, formData.fields[0].name);
    if (usernameInput == null || !__gCrWeb.common.isTextField(usernameInput) ||
        usernameInput.disabled) {
      continue;
    }
    var passwordInput =
        findInputByFieldIdentifier(inputs, formData.fields[1].name);
    if (passwordInput == null || passwordInput.type != 'password' ||
        passwordInput.readOnly || passwordInput.disabled) {
      continue;
    }

    // If username was provided on a read-only field and it matches the
    // requested username, fill the form.
    if (usernameInput.readOnly) {
      if (usernameInput.value == username) {
        __gCrWeb.fill.setInputElementValue(password, passwordInput);
        filled = true;
      }
    } else {
      __gCrWeb.fill.setInputElementValue(username, usernameInput);
      __gCrWeb.fill.setInputElementValue(password, passwordInput);
      filled = true;
    }
  }

  // Recursively invoke for all iframes.
  var frames = getSameOriginFrames(win);
  for (var i = 0; i < frames.length; i++) {
    if (fillPasswordFormWithData(
            formData, username, password, frames[i], opt_normalizedOrigin)) {
      filled = true;
    }
  }

  return filled;
};

/**
 * Finds all forms with passwords in the supplied window or frame and appends
 * JS objects containing the form data to |formDataList|.
 * @param {!Array<Object>} formDataList A list that this function populates
 *     with descriptions of discovered forms.
 * @param {Window} win A window (or frame) in which the function should
 *    look for password forms.
 */
var getPasswordFormDataList = function(formDataList, win) {
  var doc = win.document;
  var forms = doc.forms;
  for (var i = 0; i < forms.length; i++) {
    var formData = __gCrWeb.passwords.getPasswordFormData(forms[i]);
    if (formData) {
      formDataList.push(formData);
      addSubmitButtonTouchEndHandler(forms[i]);
    }
  }

  // Recursively invoke for all iframes.
  var frames = getSameOriginFrames(win);
  for (var i = 0; i < frames.length; i++) {
    getPasswordFormDataList(formDataList, frames[i]);
  }
};

/**
 * Returns a JS object containing the data from |formElement|.
 * @param {HTMLFormElement} formElement An HTML Form element.
 * @return {Object} Object of data from formElement.
 */
__gCrWeb.passwords.getPasswordFormData = function(formElement) {
  var extractMask = __gCrWeb.fill.EXTRACT_MASK_VALUE;
  var formData = {};
  var ok = __gCrWeb.fill.webFormElementToFormData(
      window, formElement, null /* formControlElement */, extractMask, formData,
      null /* field */);
  return ok ? formData : null;
};

}());  // End of anonymous object
