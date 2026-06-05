// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as fillConstants from '//components/autofill/ios/form_util/resources/fill_constants.js';
import * as fillUtil from '//components/autofill/ios/form_util/resources/fill_util.js';
import {unownedFormElementsAndFieldSetsToFormData, webFormElementToFormData} from '//components/autofill/ios/form_util/resources/fill_web_form.js';
import {getFormControlElements, getFormElementFromRendererId} from '//components/autofill/ios/form_util/resources/form_utils.js';
import {getElementByUniqueID} from '//components/autofill/ios/form_util/resources/renderer_id.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {isTextField, sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * @fileoverview Installs Passwords management functions on the gCrWeb object.
 *
 * It scans the DOM, extracting and storing password forms and returns a JSON
 * string representing an array of objects, each of which represents an Password
 * form with information about a form to be filled and/or submitted and it can
 * be translated to class FormData for further processing.
 */

/**
 * Container that holds the result from password filling.
 */
interface FillResult {
  // True when the username was actually filled. Set to false if the username
  // field isn't editable at the the time of filling, if the provided render ID
  // is 0, if the username won't change or if the fill attempt aborted.
  didFillUsername: boolean;
  // True when the password was actually filled. this will be set to false, if
  // the provided renderer ID is 0, if the password won't change, or if the fill
  // attempt aborted.
  didFillPassword: boolean;
  // True if there was an attempt to fill the form but without necessarily
  // filling the field. If one of the fields to fill isn't available at the time
  // of filling, this will be false. If the fields that had to be filled were
  // there but couldn't be filled, this will be set to true.
  didAttemptFill: boolean;
}

// Represents the FillResult when filling failed.
const kFillResultForFailure: FillResult = {
  didFillUsername: false,
  didFillPassword: false,
  didAttemptFill: false,
};

/**
 * Finds all password forms in the frame and returns form data as a JSON
 * string. Include the single username forms to support UFF.
 * @return Form data as a JSON string.
 */
function findPasswordForms(): string {
  const formDataList: fillUtil.AutofillFormData[] = [];
  getPasswordFormDataList(formDataList);
  return fillUtil.stringify(formDataList);
}

/**
 * If `form` has no submit elements and exactly 1 button that button
 * is assumed to be a submit button. This function adds onSubmitButtonClick
 * as a handler for touchend event of this button. Touchend event is used as
 * a proxy for onclick event because onclick handling might be prevented by
 * the site JavaScript.
 */
function addSubmitButtonTouchEndHandler(form: HTMLFormElement) {
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
  for (const button of buttons) {
    button.addEventListener('touchend', onSubmitButtonTouchEnd);
  }
}

/**
 * Click handler for the submit button.
 */
function onSubmitButtonTouchEnd(evt: Event) {
  const form = (evt.currentTarget as HTMLFormElement)['form'];
  const formData = getPasswordFormData(form);
  if (!formData) {
    return;
  }
  sendWebKitMessage('PasswordFormSubmitButtonClick', formData);
}

/**
 * Returns the element from `inputs` which has the field identifier equal to
 * `identifier` and null if there is no such element.
 */
function findInputByFieldRendererID(
    inputs: HTMLInputElement[], identifier: number): HTMLInputElement|null {
  if (identifier.toString() === fillConstants.RENDERER_ID_NOT_SET) {
    return null;
  }
  for (const input of inputs) {
    if (identifier.toString() === fillUtil.getUniqueID(input)) {
      return input;
    }
  }
  return null;
}

/**
 * Returns an array of input elements in a form.
 * @param form A form element for which the input elements
 *   are returned.
 */
// TODO(crbug.com/454044167): Cleanup autofill TS type casting.
function getFormInputElements(form: HTMLFormElement): HTMLInputElement[] {
  return getFormControlElements(form).filter(
      (element: Element): element is HTMLInputElement => {
        return element.tagName === 'INPUT';
      });
}

/**
 * Returns the password form with the given |identifier| as a JSON string.
 * @param identifier The identifier of the form to extract.
 * @return The password form.
 */
function getPasswordFormDataAsString(identifier: number): string {
  const hasFormTag =
      identifier.toString() !== fillConstants.RENDERER_ID_NOT_SET;
  const form = hasFormTag ? getFormElementFromRendererId(identifier) : null;
  if (!form && hasFormTag) {
    return '{}';
  }
  const formData = form ? getPasswordFormData(form) :
                          getPasswordFormDataFromUnownedElements();
  if (!formData) {
    return '{}';
  }
  return fillUtil.stringify(formData);
}

/**
 * Finds the form described by |formData| and fills in the
 * username and password values.
 *
 * This is a public function invoked by Chrome. There is no information
 * passed to this function that the page does not have access to anyway.
 *
 * @param formData Form data.
 * @param username The username to fill.
 * @param password The password to fill.
 * @return {FillResult} The result of filling the password fields.
 */

// TODO(crbug.com/454044167): Cleanup autofill TS type casting.
function fillPasswordForm(
    formData: fillUtil.AutofillFormData, username: string,
    password: string): FillResult {
  const form = getFormElementFromRendererId(Number(formData.renderer_id));
  if (form) {
    const inputs = getFormInputElements(form);
    return fillUsernameAndPassword(inputs, formData, username, password);
  }

  // Check fields that are not inside any <form> tag.
  const unownedInputs = fillUtil.getUnownedAutofillableFormFieldElements([]) as
      HTMLInputElement[];
  if (unownedInputs.length > 0) {
    return fillUsernameAndPassword(unownedInputs, formData, username, password);
  }
  return kFillResultForFailure;
}

/**
 * Finds the form identified by |formIdentifier| and fills its password fields
 * with |password|.
 *
 * @param formIdentifier The name of the form to fill.
 * @param newPasswordIdentifier The id of password element to fill.
 * @param confirmPasswordIdentifier The id of confirm password element
 *   to fill.
 * @param password The password to fill.
 * @return Whether new password field has been filled.
 */
function fillPasswordFormWithGeneratedPassword(
    formIdentifier: number, newPasswordIdentifier: number,
    confirmPasswordIdentifier: number, password: string): boolean {
  const hasFormTag =
      formIdentifier.toString() !== fillConstants.RENDERER_ID_NOT_SET;
  if (fillGeneratedPassword(
          formIdentifier, newPasswordIdentifier, confirmPasswordIdentifier,
          password, hasFormTag)) {
    return true;
  }
  return false;
}

/**
 * Fills password fields in the form identified by |formIdentifier|
 * with |password| in the current window.
 *
 * @param formIdentifier The name of the form to fill.
 * @param newPasswordIdentifier The id of password element to fill.
 * @param confirmPasswordIdentifier The id of confirm password
 *     element to fill.
 * @param password The password to fill.
 * @param hasFormTag Whether the new password field belongs to a
 *     <form> element.
 * @return Whether new password field has been filled.
 */
function fillGeneratedPassword(
    formIdentifier: number, newPasswordIdentifier: number,
    confirmPasswordIdentifier: number, password: string,
    hasFormTag: boolean): boolean {
  const form = getFormElementFromRendererId(formIdentifier);
  if (!form && hasFormTag) {
    return false;
  }
  const inputs = hasFormTag ?
      getFormInputElements(form as HTMLFormElement) :
      fillUtil.getUnownedAutofillableFormFieldElements([]) as
          HTMLInputElement[];
  const newPasswordField =
      findInputByFieldRendererID(inputs, newPasswordIdentifier);
  if (!newPasswordField) {
    return false;
  }
  // Avoid resetting if same value, as it moves cursor to the end.
  if (newPasswordField.value !== password) {
    fillUtil.setInputElementValue(password, newPasswordField);
  }
  const confirmPasswordField =
      findInputByFieldRendererID(inputs, confirmPasswordIdentifier);
  if (confirmPasswordField && confirmPasswordField.value !== password) {
    fillUtil.setInputElementValue(password, confirmPasswordField);
  }
  return true;
}

/**
 * Gets the username input element for fill.
 * @param inputs Available inputs in the form.
 * @param rendererId Renderer ID of the username input to fill.
 * @returns Input element to fill with the username or
 *     null if the input element wasn't found.
 */
function getUsernameInputElementForFill(
    inputs: HTMLInputElement[],rendererId: number): HTMLInputElement|null {
  if (rendererId === Number(fillConstants.RENDERER_ID_NOT_SET)) {
    return null;
  }
  const usernameInput = findInputByFieldRendererID(inputs, rendererId);
  if (!usernameInput) {
    return null;
  }
  if (!isTextField(usernameInput)) {
    return null;
  }
  return usernameInput;
}

/**
 * Gets the password input element for fill.
 * @param inputs Available inputs in the form.
 * @param rendererId Renderer ID of the password input to fill.
 * @returns Input element to fill with the password or
 *     null if the input element wasn't found.
 */
function getPasswordInputElementForFill(
    inputs: HTMLInputElement[], rendererId: number): HTMLInputElement|null {
  if (rendererId === Number(fillConstants.RENDERER_ID_NOT_SET)) {
    return null;
  }
  const passwordInput = findInputByFieldRendererID(inputs, rendererId);
  if (!passwordInput) {
    return null;
  }
  if (passwordInput.type !== 'password' || passwordInput.readOnly ||
      passwordInput.disabled) {
    return null;
  }
  return passwordInput;
}

/**
 * Finds target input fields in all form/formless inputs and
 * fill them with fill data.
 * @param inputs Form inputs.
 * @param formData Form data.
 * @param username The username to fill.
 * @param password The password to fill.
 * @return {FillResult} The result of filling the password fields.
 */
// TODO(crbug.com/454044167): Cleanup type casting of `usernameInput`.
function fillUsernameAndPassword(
    inputs: HTMLInputElement[], formData: fillUtil.AutofillFormData, username: string,
    password: string): FillResult {
  const usernameRendererId: number = Number(formData.fields[0]!.renderer_id);
  let usernameInput = null;
  if (usernameRendererId !== Number(fillConstants.RENDERER_ID_NOT_SET)) {
    usernameInput = getUsernameInputElementForFill(inputs, usernameRendererId);
    if (!usernameInput) {
      // Don't fill anything if the username can't be filled when it should be
      // filled.
      return kFillResultForFailure;
    }
  }

  const passwordRendererId: number = Number(formData.fields[1]!.renderer_id);
  let passwordInput;
  if (passwordRendererId !== Number(fillConstants.RENDERER_ID_NOT_SET)) {
    passwordInput = getPasswordInputElementForFill(inputs, passwordRendererId);
    if (!passwordInput) {
      // Don't fill anything if the password can't be filled when it should be
      // filled.
      return kFillResultForFailure;
    }
  }

  const isUsernameEditable: boolean = Boolean(
      !!usernameInput && !usernameInput.readOnly && !usernameInput.disabled);

  // Fill the username if needed and if it doesn't look like it was already
  // pre-filled by the website.
  const didFillUsername: boolean =
      (isUsernameEditable &&
       fillUtil.setInputElementValue(
           username, usernameInput as HTMLInputElement | null)) as boolean;

  // Fill the password if needed.
  const didFillPassword: boolean =
      Boolean(
          !!passwordInput &&
          fillUtil.setInputElementValue(password, passwordInput)) as boolean;

  return {
    didFillUsername,
    didFillPassword,
    didAttemptFill: true,
  };
}

/**
 * Returns true if the form is a recognized credential form. JS equivalent of
 * IsRendererRecognizedCredentialForm() for other platforms
 * (components/password_manager/core/common/password_manager_util.h).
 * @param form Object with the parsed form data.
 */
function isRecognizedCredentialForm(form: fillUtil.AutofillFormData) {
  return form.fields.some(
      field => field['autocomplete_attribute']?.includes('username') ||
          field['autocomplete_attribute']?.includes('webauthn') ||
          field['form_control_type'] === 'password');
}

/**
 * Finds all forms with passwords in the current window or frame and appends
 * JS objects containing the form data to |formDataList|.
 * @param formDataList A list that this function populates
 *     with descriptions of discovered forms.
 */
function getPasswordFormDataList(formDataList: fillUtil.AutofillFormData[]) {
  const forms = document.forms;
  for (const form of forms) {
    const formData = getPasswordFormData(form);
    if (formData && isRecognizedCredentialForm(formData)) {
      formDataList.push(formData);
      addSubmitButtonTouchEndHandler(form);
    }
  }
  const unownedFormData = getPasswordFormDataFromUnownedElements();
  if (unownedFormData && isRecognizedCredentialForm(unownedFormData)) {
    formDataList.push(unownedFormData);
  }
}

/**
 * Finds all forms with passwords that are not inside any <form> tag and returns
 * JS object containing the form data.
 * @return Object of data from formElement.
 */
function getPasswordFormDataFromUnownedElements(): fillUtil.AutofillFormData|
    null {
  const fieldsets: Element[] = [];
  const unownedControlElements =
      fillUtil.getUnownedAutofillableFormFieldElements(fieldsets);
  if (unownedControlElements.length === 0) {
    return null;
  }
  const unownedForm = new fillUtil.AutofillFormData();
  const hasUnownedForm = unownedFormElementsAndFieldSetsToFormData(
      window, fieldsets, unownedControlElements, /* iframeElements= */[], false,
      unownedForm);
  return hasUnownedForm ? unownedForm : null;
}

/**
 * Returns a JS object containing the data from |formElement|.
 * @param formElement An HTML Form element.
 * @return Object of data from formElement.
 */
function getPasswordFormData(
    formElement: HTMLFormElement): fillUtil.AutofillFormData|null {
  const formData = {} as fillUtil.AutofillFormData;
  const ok = webFormElementToFormData(
      window, formElement, /*formControlElement=*/ null, formData);
  return ok ? formData : null;
}

// TODO(crbug.com/500385204): Move these helpers to somewhere it can be easily
// reused for other features.

/**
 * Submits the form identified by |formIdentifier| or dispatches Enter key
 * events as last resort on the password field if standard submission fails (
 * with a small chance of success since the isTrusted bit is false for these
 * synthetic events).
 * @param formIdentifier The identifier of the form to submit.
 * @param passwordIdentifier The identifier of the password field to dispatch
 *     events on.
 * @param fallbackToKeystroke Whether to fallback to keystroke submission.
 * @return True if a submission attempt was made (e.g., via requestSubmit() or
 *     by dispatching an Enter key event). Note that returning true does not
 *     guarantee that the form was successfully submitted.
 */
function submitPasswordForm(
    formIdentifier: number, passwordIdentifier: number|null = null,
    fallbackToKeystroke: boolean = false): boolean {
  const form = getFormElementFromRendererId(formIdentifier);
  if (form) {
    try {
      form.requestSubmit();
      return true;
    } catch (e) {
      // Fallback to dispatching Enter key events if requestSubmit fails.
    }
  }

  if (fallbackToKeystroke && passwordIdentifier !== null) {
    let inputs: HTMLInputElement[];
    if (form) {
      inputs = getFormInputElements(form);
    } else {
      inputs = fillUtil.getUnownedAutofillableFormFieldElements([]) as
          HTMLInputElement[];
    }
    const passwordInput =
        findInputByFieldRendererID(inputs, passwordIdentifier);
    if (passwordInput) {
      dispatchEnterKeyEvent(passwordInput);
      return true;
    }
  }

  return false;
}

/**
 * Dispatches synthetic Enter key events on the given element.
 * @param element The element to dispatch events on.
 */
function dispatchEnterKeyEvent(element: HTMLElement): void {
  const eventInit = {
    key: 'Enter',
    code: 'Enter',
    keyCode: 13,
    which: 13,
    bubbles: true,
    cancelable: true,
    composed: true,
  };

  element.dispatchEvent(new KeyboardEvent('keydown', eventInit));
  element.dispatchEvent(new KeyboardEvent('keypress', eventInit));
  element.dispatchEvent(new KeyboardEvent('keyup', eventInit));
}

// Map to store original inputmode values
const originalInputModes = new WeakMap<HTMLElement, string>();

/**
 * Sets the inputmode of the element to 'none' to prevent showing the keyboard
 * on focus.
 */
function preventKeyboardOnElement(elementId: number): boolean {
  const element = getElementByUniqueID(elementId) as HTMLElement;
  if (!element) {
    return false;
  }

  if (element.hasAttribute('inputmode')) {
    originalInputModes.set(element, element.getAttribute('inputmode')!);
  }
  element.setAttribute('inputmode', 'none');
  return true;
}
/**
 * Restores the original inputmode of the element.
 * @param elementId Unique ID of the element to restore.
 * @return Whether the inputmode was successfully restored.
 */
function restoreKeyboardOnElement(elementId: number): boolean {
  const element = getElementByUniqueID(elementId) as HTMLElement;
  if (!element) {
    return false;
  }

  if (originalInputModes.has(element)) {
    element.setAttribute('inputmode', originalInputModes.get(element)!);
    originalInputModes.delete(element);
  } else {
    element.removeAttribute('inputmode');
  }

  // Blur the element right away to prevent the keyboard from showing up after
  // restoring the inputmode.
  element.blur();

  return true;
}

let activeShieldTargetRef: WeakRef<HTMLElement>|null = null;
let activeShieldTimeoutId: number|null = null;

/**
 * Event listener that acts as a transparent shield over the page.
 * It intercepts and prevents all user interactions (like clicks or touches)
 * that do not target the currently active shield target element.
 */
function keystrokeShieldListener(e: Event): void {
  const targetElement = activeShieldTargetRef?.deref();
  if (!targetElement) {
    return;
  }

  if (e.target !== targetElement) {
    e.preventDefault();
    e.stopPropagation();
  }
}

const RENDERER_SHIELD_TIMEOUT_MS = 1000;

/**
 * Sets up a keystroke interaction shield in the renderer to prevent
 * keystrokes from affecting elements other than the targeted element.
 * @param elementId Unique ID of the element to protect.
 * @return Whether the shield was successfully set up.
 */
function setUpRendererKeystrokeShield(elementId: number): boolean {
  removeRendererKeystrokeShield();  // Ensure any existing shield is cleaned up

  const element = getElementByUniqueID(elementId) as HTMLElement;
  if (!element) {
    return false;
  }

  activeShieldTargetRef = new WeakRef(element);

  // Use capture phase to intercept the event before it reaches other elements.
  document.addEventListener('keydown', keystrokeShieldListener, true);
  document.addEventListener('keypress', keystrokeShieldListener, true);
  document.addEventListener('keyup', keystrokeShieldListener, true);

  // Fallback to clear the shield after 1 second if not explicitly removed.
  activeShieldTimeoutId = window.setTimeout(() => {
    removeRendererKeystrokeShield();
  }, RENDERER_SHIELD_TIMEOUT_MS);

  return true;
}

/**
 * Removes the keystroke interaction shield.
 */
function removeRendererKeystrokeShield(): void {
  if (activeShieldTimeoutId !== null) {
    window.clearTimeout(activeShieldTimeoutId);
    activeShieldTimeoutId = null;
  }

  activeShieldTargetRef = null;

  document.removeEventListener('keydown', keystrokeShieldListener, true);
  document.removeEventListener('keypress', keystrokeShieldListener, true);
  document.removeEventListener('keyup', keystrokeShieldListener, true);
}

/**
 * Focuses the element.
 * @returns true if the element was successfully found and focused, false
 *     otherwise.
 */
function focusElement(elementId: number): boolean {
  const element = getElementByUniqueID(elementId) as HTMLElement;
  if (element) {
    element.focus();
    return true;
  }
  return false;
}


// TODO(crbug.com/454044167): Cleanup autofill TS type casting.
/**
 * Finds the form described by |formData| and fills in the
 * username and password values. Then triggers form submission.
 *
 * @param formData Form data.
 * @param username The username to fill.
 * @param password The password to fill.
 * @param fallbackToKeystroke Whether to fallback to keystroke submission.
 * @return {FillResult} The result of filling the password fields.
 */
function fillPasswordFormAndSubmit(
    formData: fillUtil.AutofillFormData, username: string, password: string,
    fallbackToKeystroke: boolean = false): FillResult {
  const result = fillPasswordForm(formData, username, password);
  if (result.didAttemptFill) {
    // Extract password renderer ID if available from formData
    let passwordRendererId: number|null = null;
    if (formData.fields && formData.fields.length > 1 &&
        formData.fields[1]!.renderer_id) {
      passwordRendererId = Number(formData.fields[1]!.renderer_id);
    }
    submitPasswordForm(
        Number(formData.renderer_id), passwordRendererId, fallbackToKeystroke);
  }
  return result;
}

const passwordsApi = new CrWebApi('passwords');

passwordsApi.addFunction('findPasswordForms', findPasswordForms);
passwordsApi.addFunction('fillPasswordForm', fillPasswordForm);
passwordsApi.addFunction(
    'fillPasswordFormWithGeneratedPassword',
    fillPasswordFormWithGeneratedPassword);
passwordsApi.addFunction('getPasswordFormData', getPasswordFormData);
passwordsApi.addFunction(
    'getPasswordFormDataAsString', getPasswordFormDataAsString);
passwordsApi.addFunction('submitPasswordForm', submitPasswordForm);
passwordsApi.addFunction(
    'fillPasswordFormAndSubmit', fillPasswordFormAndSubmit);
passwordsApi.addFunction('preventKeyboardOnElement', preventKeyboardOnElement);
passwordsApi.addFunction('restoreKeyboardOnElement', restoreKeyboardOnElement);
passwordsApi.addFunction(
    'setUpRendererKeystrokeShield', setUpRendererKeystrokeShield);
passwordsApi.addFunction(
    'removeRendererKeystrokeShield', removeRendererKeystrokeShield);
passwordsApi.addFunction('focusElement', focusElement);

gCrWeb.registerApi(passwordsApi);
