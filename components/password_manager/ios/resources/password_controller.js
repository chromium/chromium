// Copyright 2012 The Chromium Authors
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

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected.
 */
__gCrWeb.passwords = {};
__gCrWeb['passwords'] = __gCrWeb.passwords;

/**
 * Finds all password forms in the frame and returns form data as a JSON
 * string.
 * @return {string} Form data as a JSON string.
 */
__gCrWeb.passwords['findPasswordForms'] = function() {
  const formDataList = [];
  if (hasPasswordField()) {
    getPasswordFormDataList(formDataList);
  }
  return __gCrWeb.stringify(formDataList);
};

/**
 * Returns true if the current window contains an input field of type
 * 'password'.
 * @private
 * @return {boolean}
 */
const hasPasswordField = function() {
  // We may will not be allowed to read the 'document' property from a frame
  // that is in a different domain.
  if (!document) {
    return false;
  }
  return document.querySelector('input[type=password]');
};

/**
 * If |form| has no submit elements and exactly 1 button that button
 * is assumed to be a submit button. This function adds onSubmitButtonClick_
 * as a handler for touchend event of this button. Touchend event is used as
 * a proxy for onclick event because onclick handling might be prevented by
 * the site JavaScript.
 */
const addSubmitButtonTouchEndHandler = function(form) {
  if (form.querySelector('input[type=submit]')) {
    return;
  }
  // Try to find buttons of type submit at first.
  let buttons = form.querySelectorAll('button[type="submit"]');
  if (buttons.length === 0) {
    // Try to check all buttons. If there is only one button, assume that this
    // is the submit button.
    buttons = form.querySelectorAll('button');
    if (buttons.length !== 1) {
      return;
    }
  }
  for (let i = 0; i < buttons.length; ++i) {
    buttons[0].addEventListener('touchend', onSubmitButtonTouchEnd);
  }
};

/**
 * Click handler for the submit button. It sends to the host
 * form.submitButtonClick command.
 */
const onSubmitButtonTouchEnd = function(evt) {
  const form = evt.currentTarget.form;
  const formData = __gCrWeb.passwords.getPasswordFormData(form);
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
 * @param {number} identifier
 * @return {HTMLInputElement}
 */
const findInputByUniqueFieldId = function(inputs, identifier) {
  if (identifier.toString() === __gCrWeb.fill.RENDERER_ID_NOT_SET) {
    return null;
  }
  for (let i = 0; i < inputs.length; ++i) {
    if (identifier.toString() === __gCrWeb.fill.getUniqueID(inputs[i])) {
      return inputs[i];
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
const getFormInputElements = function(form) {
  return __gCrWeb.form.getFormControlElements(form).filter(function(element) {
    return element.tagName === 'INPUT';
  });
};

/**
 * Returns the password form with the given |identifier| as a JSON string.
 * @param {number} identifier The identifier of the form to extract.
 * @return {string} The password form.
 */
__gCrWeb.passwords['getPasswordFormDataAsString'] = function(identifier) {
  const hasFormTag =
      identifier.toString() !== __gCrWeb.fill.RENDERER_ID_NOT_SET;
  const form = hasFormTag ?
      __gCrWeb.form.getFormElementFromUniqueFormId(identifier) :
      null;
  if (!form && hasFormTag) {
    return '{}';
  }
  const formData = hasFormTag ?
      __gCrWeb.passwords.getPasswordFormData(form) :
      __gCrWeb.passwords.getPasswordFormDataFromUnownedElements();
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
 * @return {boolean} Whether a form field has been filled.
 */
__gCrWeb.passwords['fillPasswordForm'] = function(
    formData, username, password) {
  let filled = false;

  const form =
      __gCrWeb.form.getFormElementFromUniqueFormId(formData.unique_renderer_id);
  if (form) {
    const inputs = getFormInputElements(form);
    if (fillUsernameAndPassword_(inputs, formData, username, password)) {
      filled = true;
    }
  }

  // Check fields that are not inside any <form> tag.
  const unownedInputs =
      __gCrWeb.fill.getUnownedAutofillableFormFieldElements(document.all, []);
  if (fillUsernameAndPassword_(unownedInputs, formData, username, password)) {
    filled = true;
  }

  return filled;
};

/**
 * Finds the form identified by |formIdentifier| and fills its password fields
 * with |password|.
 *
 * @param {number} formIdentifier The name of the form to fill.
 * @param {number} newPasswordIdentifier The id of password element to fill.
 * @param {number} confirmPasswordIdentifier The id of confirm password element
 *   to fill.
 * @param {string} password The password to fill.
 * @return {boolean} Whether new password field has been filled.
 */
__gCrWeb.passwords['fillPasswordFormWithGeneratedPassword'] = function(
    formIdentifier, newPasswordIdentifier, confirmPasswordIdentifier,
    password) {
  const hasFormTag =
      formIdentifier.toString() !== __gCrWeb.fill.RENDERER_ID_NOT_SET;
  if (fillGeneratedPassword(
          formIdentifier, newPasswordIdentifier, confirmPasswordIdentifier,
          password, hasFormTag)) {
    return true;
  }
  return false;
};

/**
 * Fills password fields in the form identified by |formIdentifier|
 * with |password| in the current window.
 *
 * @param {number} formIdentifier The name of the form to fill.
 * @param {number} newPasswordIdentifier The id of password element to fill.
 * @param {number} confirmPasswordIdentifier The id of confirm password
 *     element to fill.
 * @param {string} password The password to fill.
 * @param {boolean} hasFormTag Whether the new password field belongs to a
 *     <form> element.
 * @return {boolean} Whether new password field has been filled.
 */
const fillGeneratedPassword = function(
    formIdentifier, newPasswordIdentifier, confirmPasswordIdentifier, password,
    hasFormTag) {
  const form = __gCrWeb.form.getFormElementFromUniqueFormId(formIdentifier);
  if (!form && hasFormTag) {
    return false;
  }
  const inputs = hasFormTag ?
      getFormInputElements(form) :
      __gCrWeb.fill.getUnownedAutofillableFormFieldElements(document.all, []);
  const newPasswordField =
      findInputByUniqueFieldId(inputs, newPasswordIdentifier);
  if (!newPasswordField) {
    return false;
  }
  // Avoid resetting if same value, as it moves cursor to the end.
  if (newPasswordField.value !== password) {
    __gCrWeb.fill.setInputElementValue(password, newPasswordField);
  }
  const confirmPasswordField =
      findInputByUniqueFieldId(inputs, confirmPasswordIdentifier);
  if (confirmPasswordField && confirmPasswordField.value !== password) {
    __gCrWeb.fill.setInputElementValue(password, confirmPasswordField);
  }
  return true;
};

/**
 * Finds target input fields in all form/formless inputs and
 * fill them with fill data.
 * @param {Array<FormControlElement>} inputs Form inputs.
 * @param {AutofillFormData} formData Form data.
 * @param {string} username The username to fill.
 * @param {string} password The password to fill.
 * @return {boolean} Whether the form has been filled.
 */
function fillUsernameAndPassword_(inputs, formData, username, password) {
  const usernameIdentifier = formData.fields[0].unique_renderer_id;
  let usernameInput = null;
  if (usernameIdentifier !== Number(__gCrWeb.fill.RENDERER_ID_NOT_SET)) {
    usernameInput = findInputByUniqueFieldId(inputs, usernameIdentifier);
    if (!usernameInput || !__gCrWeb.common.isTextField(usernameInput)) {
      return false;
    }
  }
  const passwordInput =
      findInputByUniqueFieldId(inputs, formData.fields[1].unique_renderer_id);
  if (!passwordInput || passwordInput.type !== 'password' ||
      passwordInput.readOnly || passwordInput.disabled) {
    return false;
  }
  // If username was provided on a read-only or disabled field, fill the form.
  if (!(usernameInput && (usernameInput.readOnly || usernameInput.disabled))) {
    __gCrWeb.fill.setInputElementValue(username, usernameInput);
  }

  __gCrWeb.fill.setInputElementValue(password, passwordInput);
  return true;
}

/**
 * Finds all forms with passwords in the current window or frame and appends
 * JS objects containing the form data to |formDataList|.
 * @param {!Array<Object>} formDataList A list that this function populates
 *     with descriptions of discovered forms.
 */
const getPasswordFormDataList = function(formDataList) {
  const forms = document.forms;
  for (let i = 0; i < forms.length; i++) {
    const formData = __gCrWeb.passwords.getPasswordFormData(forms[i]);
    if (formData) {
      formDataList.push(formData);
      addSubmitButtonTouchEndHandler(forms[i]);
    }
  }
  const unownedFormData =
      __gCrWeb.passwords.getPasswordFormDataFromUnownedElements();
  if (unownedFormData) {
    formDataList.push(unownedFormData);
  }
};

/**
 * Finds all forms with passwords that are not inside any <form> tag and returns
 * JS object containing the form data.
 * @return {Object} Object of data from formElement.
 */
__gCrWeb.passwords.getPasswordFormDataFromUnownedElements = function() {
  const extractMask = __gCrWeb.fill.EXTRACT_MASK_VALUE;
  const fieldsets = [];
  const unownedControlElements =
      __gCrWeb.fill.getUnownedAutofillableFormFieldElements(
          document.all, fieldsets);
  if (unownedControlElements.length === 0) {
    return;
  }
  const unownedForm = new __gCrWeb['common'].JSONSafeObject();
  const hasUnownedForm =
      __gCrWeb.fill.unownedFormElementsAndFieldSetsToFormData(
          window, fieldsets, unownedControlElements, extractMask, false,
          unownedForm);
  return hasUnownedForm ? unownedForm : null;
};


/**
 * Returns a JS object containing the data from |formElement|.
 * @param {HTMLFormElement} formElement An HTML Form element.
 * @return {Object} Object of data from formElement.
 */
__gCrWeb.passwords.getPasswordFormData = function(formElement) {
  const extractMask = __gCrWeb.fill.EXTRACT_MASK_VALUE;
  const formData = {};
  const ok = __gCrWeb.fill.webFormElementToFormData(
      window, formElement, null /* formControlElement */, extractMask, formData,
      null /* field */);
  return ok ? formData : null;
};
