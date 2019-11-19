// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
  * @fileoverview Installs Autofill management functions on the __gCrWeb object.
  *
  * It scans the DOM, extracting and storing forms and returns a JSON string
  * representing an array of objects, each of which represents an Autofill form
  * with information about a form to be filled and/or submitted and it can be
  * translated to struct FormData
  * (chromium/src/components/autofill/core/common/form_data.h) for further
  * processing.

  * TODO(crbug.com/647084): Enable checkTypes error for this file.
  * @suppress {checkTypes}
  */
goog.provide('__crWeb.autofill');

/**
 * The autofill data for a form.
 * @typedef {{
 *   formName: string,
 *   fields: !Object<string, !Object<string, string>>,
 * }}
 */
// eslint-disable-next-line no-var
var FormData;

/* Beginning of anonymous object. */
(function() {

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected.
 */
__gCrWeb.autofill = {};

// Store autofill namespace object in a global __gCrWeb object referenced by a
// string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['autofill'] = __gCrWeb.autofill;

/**
 * The delay between filling two fields
 *
 * Page need time to propagate the events after setting one field. Add a delay
 * between filling two fields. In milliseconds.
 *
 * @type {number}
 */
__gCrWeb.autofill.delayBetweenFieldFillingMs = 50;

/**
 * The last element that was autofilled.
 *
 * @type {Element}
 */
__gCrWeb.autofill.lastAutoFilledElement = null;

/**
 * Whether CSS for autofilled elements has been injected into the page.
 *
 * @type {boolean}
 */
__gCrWeb.autofill.styleInjected = false;

/**
 * Sets the delay between fields when autofilling forms.
 *
 * @param {number} delay The new delay in milliseconds.
 */
__gCrWeb.autofill.setDelay = function(delay) {
  __gCrWeb.autofill.delayBetweenFieldFillingMs = delay;
};

/**
 * Searches an element's ancestors to see if the element is inside a <form> or
 * <fieldset>.
 *
 * It is based on the logic in
 *     bool IsElementInsideFormOrFieldSet(const WebElement& element)
 * in chromium/src/components/autofill/content/renderer/form_cache.cc
 *
 * @param {!FormControlElement} element An element to examine.
 * @return {boolean} Whether the element is inside a <form> or <fieldset>.
 */
function isElementInsideFormOrFieldSet(element) {
  let parentNode = element.parentNode;
  while (parentNode) {
    if ((parentNode.nodeType === Node.ELEMENT_NODE) &&
        (__gCrWeb.fill.hasTagName(parentNode, 'form') ||
         __gCrWeb.fill.hasTagName(parentNode, 'fieldset'))) {
      return true;
    }
    parentNode = parentNode.parentNode;
  }
  return false;
}

/**
 * Determines whether the form is interesting enough to send to the browser for
 * further operations.
 *
 * Unlike the C++ version, this version takes a required field count param,
 * instead of using a hard coded value.
 *
 * It is based on the logic in
 *     bool IsFormInteresting(const FormData& form,
 *                            size_t num_editable_elements);
 * in chromium/src/components/autofill/content/renderer/form_cache.cc
 *
 * @param {AutofillFormData} form Form to examine.
 * @param {number} numEditableElements number of editable elements.
 * @param {number} numFieldsRequired number of fields required.
 * @return {boolean} Whether the form is sufficiently interesting.
 */
function isFormInteresting_(form, numEditableElements, numFieldsRequired) {
  if (form.fields.length === 0) {
    return false;
  }

  // If the form has at least one field with an autocomplete attribute, it is a
  // candidate for autofill.
  for (let i = 0; i < form.fields.length; ++i) {
    if (form.fields[i]['autocomplete_attribute'] != null &&
        form.fields[i]['autocomplete_attribute'].length > 0) {
      return true;
    }
  }

  // If there are no autocomplete attributes, the form needs to have at least
  // the required number of editable fields for the prediction routines to be a
  // candidate for autofill.
  return numEditableElements >= numFieldsRequired;
}

/**
 * Scans |control_elements| and returns the number of editable elements.
 *
 * Unlike the C++ version, this version does not take the
 * log_deprecation_messages parameter, and it does not save any state since
 * there is no caching.
 *
 * It is based on the logic in:
 *     size_t FormCache::ScanFormControlElements(
 *         const std::vector<WebFormControlElement>& control_elements,
 *         bool log_deprecation_messages);
 * in chromium/src/components/autofill/content/renderer/form_cache.cc.
 *
 * @param {Array<FormControlElement>} controlElements The elements to scan.
 * @return {number} The number of editable elements.
 */
function scanFormControlElements_(controlElements) {
  let numEditableElements = 0;
  for (let elementIndex = 0; elementIndex < controlElements.length;
       ++elementIndex) {
    const element = controlElements[elementIndex];
    if (!__gCrWeb.fill.isCheckableElement(element)) {
      ++numEditableElements;
    }
  }
  return numEditableElements;
}

/**
 * Get all form control elements from |elements| that are not part of a form.
 * Also append the fieldsets encountered that are not part of a form to
 * |fieldsets|.
 *
 * It is based on the logic in:
 *     std::vector<WebFormControlElement>
 *     GetUnownedAutofillableFormFieldElements(
 *         const WebElementCollection& elements,
 *         std::vector<WebElement>* fieldsets);
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * In the C++ version, |fieldsets| can be NULL, in which case we do not try to
 * append to it.
 *
 * @param {Array<!FormControlElement>} elements elements to look through.
 * @param {Array<Element>} fieldsets out param for unowned fieldsets.
 * @return {Array<FormControlElement>} The elements that are not part of a form.
 */
function getUnownedAutofillableFormFieldElements_(elements, fieldsets) {
  const unownedFieldsetChildren = [];
  for (let i = 0; i < elements.length; ++i) {
    if (__gCrWeb.form.isFormControlElement(elements[i])) {
      if (!elements[i].form) {
        unownedFieldsetChildren.push(elements[i]);
      }
    }

    if (__gCrWeb.fill.hasTagName(elements[i], 'fieldset') &&
        !isElementInsideFormOrFieldSet(elements[i])) {
      fieldsets.push(elements[i]);
    }
  }
  return __gCrWeb.autofill.extractAutofillableElementsFromSet(
      unownedFieldsetChildren);
}

/**
 * Scans DOM and returns a JSON string representation of forms and form
 * extraction results. This is just a wrapper around extractNewForms() to JSON
 * encode the forms, for convenience.
 *
 * @param {number} requiredFields The minimum number of fields forms must have
 *     to be extracted.
 * @param {bool} restrictUnownedFieldsToFormlessCheckout whether forms made of
 *     unowned fields (i.e., not within a <form> tag) should be restricted to
 *     those that appear to be in a checkout flow.
 * @return {string} A JSON encoded an array of the forms data.
 */
__gCrWeb.autofill['extractForms'] = function(
    requiredFields, restrictUnownedFieldsToFormlessCheckout) {
  const forms = __gCrWeb.autofill.extractNewForms(
      requiredFields, restrictUnownedFieldsToFormlessCheckout);
  return __gCrWeb.stringify(forms);
};

/**
 * Fills data into the active form field.
 *
 * @param {AutofillFormFieldData} data The data to fill in.
 */
__gCrWeb.autofill['fillActiveFormField'] = function(data) {
  const activeElement = document.activeElement;
  if (data['identifier'] !== __gCrWeb.form.getFieldIdentifier(activeElement)) {
    return;
  }
  __gCrWeb.autofill.lastAutoFilledElement = activeElement;
  __gCrWeb.autofill.fillFormField(data, activeElement);
};

// Remove Autofill styling when control element is edited by the user.
function controlElementInputListener_(evt) {
  if (evt.isTrusted) {
    evt.target.removeAttribute('chrome-autofilled');
    evt.target.isAutofilled = false;
    evt.target.removeEventListener('input', controlElementInputListener_);
  }
}

/**
 * Fills a number of fields in the same named form for full-form Autofill.
 * Applies Autofill CSS (i.e. yellow background) to filled elements.
 * Only empty fields will be filled, except that field named
 * |forceFillFieldName| will always be filled even if non-empty.
 *
 * @param {!FormData} data Autofill data to fill in.
 * @param {string} forceFillFieldIdentifier Identified field will always be
 *     filled even if non-empty. May be null.
 */
__gCrWeb.autofill['fillForm'] = function(data, forceFillFieldIdentifier) {
  // Inject CSS to style the autofilled elements with a yellow background.
  if (!__gCrWeb.autofill.styleInjected) {
    const style = document.createElement('style');
    style.textContent = '[chrome-autofilled] {' +
        'background-color:#E8F0FE !important;' +
        'background-image:none !important;' +
        'color:#000000 !important;' +
        '}';
    document.head.appendChild(style);
    __gCrWeb.autofill.styleInjected = true;
  }

  const form = __gCrWeb.form.getFormElementFromIdentifier(data.formName);
  const controlElements = form ?
      __gCrWeb.form.getFormControlElements(form) :
      getUnownedAutofillableFormFieldElements_(document.all, /*fieldsets=*/[]);

  for (let i = 0, delay = 0; i < controlElements.length; ++i) {
    const element = controlElements[i];
    if (!__gCrWeb.fill.isAutofillableElement(element)) {
      continue;
    }

    // TODO(crbug.com/836013): Investigate autofilling checkable elements.
    if (__gCrWeb.fill.isCheckableElement(element)) {
      continue;
    }

    // Skip fields for which autofill data is missing.
    const fieldIdentifier = __gCrWeb.form.getFieldIdentifier(element);
    const fieldData = data.fields[fieldIdentifier];
    if (!fieldData) {
      continue;
    }

    // Skip non-empty fields unless:
    // a) The element's identifier matches |forceFillFieldIdentifier|; or
    // b) The element is a 'select-one' element. 'select-one' elements are
    //    always autofilled; see AutofillManager::FillOrPreviewDataModelForm().
    // c) The "value" or "placeholder" attributes match the value, if any; or
    // d) The value has not been set by the user.
    if (element.value && __gCrWeb.form.fieldWasEditedByUser(element) &&
        !__gCrWeb.autofill.sanitizedFieldIsEmpty(element.value) &&
        fieldIdentifier !== forceFillFieldIdentifier &&
        !__gCrWeb.fill.isSelectElement(element) &&
        !((element.hasAttribute('value') &&
           element.getAttribute('value') == element.value) ||
          (element.hasAttribute('placeholder') &&
           element.getAttribute('placeholder').toLowerCase() ==
               element.value.toLowerCase()))) {
      continue;
    }

    (function(_element, _value, _section, _delay) {
      window.setTimeout(function() {
        __gCrWeb.fill.setInputElementValue(_value, _element, function() {
          _element.setAttribute('chrome-autofilled', '');
          _element.isAutofilled = true;
          _element.autofillSection = _section;
          _element.addEventListener('input', controlElementInputListener_);
        });
      }, _delay);
    })(element, fieldData.value, fieldData.section, delay);
    delay += __gCrWeb.autofill.delayBetweenFieldFillingMs;
  }

  if (form) {
    // Remove Autofill styling when form receives 'reset' event.
    // Individual control elements may be left with 'input' event listeners but
    // they are harmless.
    const formResetListener = function(evt) {
      const controlElements = __gCrWeb.form.getFormControlElements(evt.target);
      for (let i = 0; i < controlElements.length; ++i) {
        controlElements[i].removeAttribute('chrome-autofilled');
        controlElements[i].isAutofilled = false;
      }
      evt.target.removeEventListener('reset', formResetListener);
    };
    form.addEventListener('reset', formResetListener);
  }
};

/**
 * Clear autofilled fields of the specified form section. Fields that are not
 * currently autofilled or do not belong to the same section as that of the
 * field with |fieldIdentifier| are not modified. If the field identified by
 * |fieldIdentifier| cannot be found all autofilled form fields get cleared.
 * Field contents are cleared, and Autofill flag and styling are removed.
 * 'change' events are sent for fields whose contents changed.
 * Based on FormCache::ClearSectionWithElement().
 *
 * @param {string} formName Identifier for form element (from
 *     getFormIdentifier).
 * @param {string} fieldIdentifier Identifier for form field initiating the
 *     clear action.
 */
__gCrWeb.autofill['clearAutofilledFields'] = function(
    formName, fieldIdentifier) {
  const form = __gCrWeb.form.getFormElementFromIdentifier(formName);
  const controlElements = form ?
      __gCrWeb.form.getFormControlElements(form) :
      getUnownedAutofillableFormFieldElements_(document.all, /*fieldsets=*/[]);

  let formField = null;
  for (let i = 0; i < controlElements.length; ++i) {
    if (__gCrWeb.form.getFieldIdentifier(controlElements[i]) ==
        fieldIdentifier) {
      formField = controlElements[i];
      break;
    }
  }

  for (let i = 0, delay = 0; i < controlElements.length; ++i) {
    const element = controlElements[i];
    if (!element.isAutofilled || element.disabled) {
      continue;
    }

    if (formField && formField.autofillSection != element.autofillSection) {
      continue;
    }

    let value = null;
    if (__gCrWeb.fill.isTextInput(element) ||
        __gCrWeb.fill.isTextAreaElement(element)) {
      value = '';
    } else if (__gCrWeb.fill.isSelectElement(element)) {
      // Reset to the first index.
      // TODO(bondd): Store initial values and reset to the correct one here.
      value = element.options[0].value;
    } else if (__gCrWeb.fill.isCheckableElement(element)) {
      // TODO(crbug.com/836013): Investigate autofilling checkable elements.
    }
    if (value !== null) {
      (function(_element, _value, _delay) {
        window.setTimeout(function() {
          __gCrWeb.fill.setInputElementValue(
              _value, _element, function(changed) {
                _element.removeAttribute('chrome-autofilled');
                _element.isAutofilled = false;
                _element.removeEventListener(
                    'input', controlElementInputListener_);
              });
        }, _delay);
      })(element, value, delay);
      delay += __gCrWeb.autofill.delayBetweenFieldFillingMs;
    }
  }
};

/**
 * Scans the DOM in |frame| extracting and storing forms. Fills |forms| with
 * extracted forms.
 *
 * This method is based on the logic in method:
 *
 *     std::vector<FormData> ExtractNewForms();
 *
 * in chromium/src/components/autofill/content/renderer/form_cache.cc.
 *
 * The difference is in this implementation, the cache is not considered.
 * Initial values of select and checkable elements are not recorded at the
 * moment.
 *
 * This version still takes the minimumRequiredFields parameters. Whereas the
 * C++ version does not.
 *
 * @param {number} minimumRequiredFields The minimum number of fields a form
 *     should contain for autofill.
 * @param {bool} restrictUnownedFieldsToFormlessCheckout whether forms made of
 *     unowned fields (i.e., not within a <form> tag) should be restricted to
 *     those that appear to be in a checkout flow.
 * @return {Array<AutofillFormData>} The extracted forms.
 */
__gCrWeb.autofill.extractNewForms = function(
    minimumRequiredFields, restrictUnownedFieldsToFormlessCheckout) {
  const forms = [];
  // Protect against custom implementation of Array.toJSON in host pages.
  /** @suppress {checkTypes} */ (function() {
    forms.toJSON = null;
  })();

  /** @type {HTMLCollection} */
  const webForms = document.forms;

  const extractMask =
      __gCrWeb.fill.EXTRACT_MASK_VALUE | __gCrWeb.fill.EXTRACT_MASK_OPTIONS;
  let numFieldsSeen = 0;
  for (let formIndex = 0; formIndex < webForms.length; ++formIndex) {
    /** @type {HTMLFormElement} */
    const formElement = webForms[formIndex];
    const controlElements =
        __gCrWeb.autofill.extractAutofillableElementsInForm(formElement);
    const numEditableElements = scanFormControlElements_(controlElements);

    if (numEditableElements === 0) {
      continue;
    }

    const form = new __gCrWeb['common'].JSONSafeObject;
    if (!__gCrWeb.fill.webFormElementToFormData(
            window, formElement, null, extractMask, form, null /* field */)) {
      continue;
    }

    numFieldsSeen += form['fields'].length;
    if (numFieldsSeen > __gCrWeb.fill.MAX_PARSEABLE_FIELDS) {
      break;
    }

    if (isFormInteresting_(form, numEditableElements, minimumRequiredFields)) {
      forms.push(form);
    }
  }

  // Look for more parseable fields outside of forms.
  const fieldsets = [];
  const unownedControlElements =
      getUnownedAutofillableFormFieldElements_(document.all, fieldsets);
  const numEditableUnownedElements =
      scanFormControlElements_(unownedControlElements);
  if (numEditableUnownedElements > 0) {
    const unownedForm = new __gCrWeb['common'].JSONSafeObject;
    const hasUnownedForm = unownedFormElementsAndFieldSetsToFormData_(
        window, fieldsets, unownedControlElements, extractMask,
        restrictUnownedFieldsToFormlessCheckout, unownedForm);
    if (hasUnownedForm) {
      numFieldsSeen += unownedForm['fields'].length;
      if (numFieldsSeen <= __gCrWeb.fill.MAX_PARSEABLE_FIELDS) {
        const interesting = isFormInteresting_(
            unownedForm, numEditableUnownedElements, minimumRequiredFields);
        if (interesting) {
          forms.push(unownedForm);
        }
      }
    }
  }
  return forms;
};

/**
 * Fills |form| with the form data object corresponding to the unowned elements
 * and fieldsets in the document.
 * |extract_mask| controls what data is extracted.
 * Returns true if |form| is filled out. Returns false if there are no fields or
 * too many fields in the |form|.
 *
 * It is based on the logic in
 *     bool UnownedFormElementsAndFieldSetsToFormData(
 *         const std::vector<blink::WebElement>& fieldsets,
 *         const std::vector<blink::WebFormControlElement>& control_elements,
 *         const GURL& origin,
 *         ExtractMask extract_mask,
 *         FormData* form)
 * and
 *     bool UnownedCheckoutFormElementsAndFieldSetsToFormData(
 *         const std::vector<blink::WebElement>& fieldsets,
 *         const std::vector<blink::WebFormControlElement>& control_elements,
 *         const blink::WebFormControlElement* element,
 *         const blink::WebDocument& document,
 *         ExtractMask extract_mask,
 *         FormData* form,
 *         FormFieldData* field)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc
 *
 * @param {HTMLFrameElement|Window} frame The window or frame where the
 *     formElement is in.
 * @param {Array<Element>} fieldsets The fieldsets to look through.
 * @param {Array<FormControlElement>} controlElements The control elements that
 *     will be processed.
 * @param {number} extractMask Mask controls what data is extracted from
 *     formElement.
 * @param {bool} restrictUnownedFieldsToFormlessCheckout whether forms made of
 *     unowned fields (i.e., not within a <form> tag) should be restricted to
 *     those that appear to be in a checkout flow.
 * @param {AutofillFormData} form Form to fill in the AutofillFormData
 *     information of formElement.
 * @return {boolean} Whether there are fields and not too many fields in the
 *     form.
 */
function unownedFormElementsAndFieldSetsToFormData_(
    frame, fieldsets, controlElements, extractMask,
    restrictUnownedFieldsToFormlessCheckout, form) {
  if (!frame) {
    return false;
  }

  form['name'] = '';
  form['origin'] =
      __gCrWeb.common.removeQueryAndReferenceFromURL(frame.location.href);
  form['action'] = '';
  form['is_form_tag'] = false;

  if (!restrictUnownedFieldsToFormlessCheckout) {
    return __gCrWeb.fill.formOrFieldsetsToFormData(
        null /* formElement*/, null /* formControlElement */, fieldsets,
        controlElements, extractMask, form, null /* field */);
  }

  // For now this restriction only applies to English-language pages, because
  // the keywords are not translated. Note that an empty "lang" attribute
  // counts as English.
  if (document.documentElement.hasAttribute('lang') &&
      !document.documentElement.getAttribute('lang').toLowerCase().startsWith(
          'en')) {
    return __gCrWeb.fill.formOrFieldsetsToFormData(
        null /* formElement*/, null /* formControlElement */, fieldsets,
        controlElements, extractMask, form, null /* field */);
  }

  const title = document.title.toLowerCase();
  const path = document.location.pathname.toLowerCase();
  // The keywords are defined in
  // UnownedCheckoutFormElementsAndFieldSetsToFormData in
  // components/autofill/content/renderer/form_autofill_util.cc
  const keywords =
      ['payment', 'checkout', 'address', 'delivery', 'shipping', 'wallet'];

  const count = keywords.length;
  for (let index = 0; index < count; index++) {
    const keyword = keywords[index];
    if (title.includes(keyword) || path.includes(keyword)) {
      form['is_formless_checkout'] = true;
      return __gCrWeb.fill.formOrFieldsetsToFormData(
          null /* formElement*/, null /* formControlElement */, fieldsets,
          controlElements, extractMask, form, null /* field */);
    }
  }

  // Since it's not a checkout flow, only add fields that have a non-"off"
  // autocomplete attribute to the formless autofill.
  const controlElementsWithAutocomplete = [];
  for (let index = 0; index < controlElements.length; index++) {
    if (controlElements[index].hasAttribute('autocomplete') &&
        controlElements[index].getAttribute('autocomplete') !== 'off') {
      controlElementsWithAutocomplete.push(controlElements[index]);
    }
  }

  if (controlElementsWithAutocomplete.length == 0) {
    return false;
  }
  return __gCrWeb.fill.formOrFieldsetsToFormData(
      null /* formElement*/, null /* formControlElement */, fieldsets,
      controlElementsWithAutocomplete, extractMask, form, null /* field */);
}

/**
 * Sets the |field|'s value to the value in |data|.
 * Also sets the "autofilled" attribute.
 *
 * It is based on the logic in
 *     void FillFormField(const FormFieldData& data,
 *                        bool is_initiating_node,
 *                        blink::WebFormControlElement* field)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * Different from FillFormField(), is_initiating_node is not considered in
 * this implementation.
 *
 * @param {AutofillFormFieldData} data Data that will be filled into field.
 * @param {FormControlElement} field The element to which data will be filled.
 */
__gCrWeb.autofill.fillFormField = function(data, field) {
  // Nothing to fill.
  if (!data['value'] || data['value'].length === 0) {
    return;
  }

  if (__gCrWeb.fill.isTextInput(field) ||
      __gCrWeb.fill.isTextAreaElement(field)) {
    let sanitizedValue = data['value'];

    if (__gCrWeb.fill.isTextInput(field)) {
      // If the 'max_length' attribute contains a negative value, the default
      // maxlength value is used.
      let maxLength = data['max_length'];
      if (maxLength < 0) {
        maxLength = __gCrWeb.fill.MAX_DATA_LENGTH;
      }
      sanitizedValue = data['value'].substr(0, maxLength);
    }

    __gCrWeb.fill.setInputElementValue(sanitizedValue, field);
    field.isAutofilled = true;
  } else if (__gCrWeb.fill.isSelectElement(field)) {
    __gCrWeb.fill.setInputElementValue(data['value'], field);
  } else if (__gCrWeb.fill.isCheckableElement(field)) {
    __gCrWeb.fill.setInputElementValue(data['is_checked'], field);
  }
};

/**
 * Returns the auto-fillable form control elements in |formElement|.
 *
 * It is based on the logic in:
 *     std::vector<blink::WebFormControlElement>
 *     ExtractAutofillableElementsFromSet(
 *         const WebVector<WebFormControlElement>& control_elements);
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {Array<FormControlElement>} controlElements Set of control elements.
 * @return {Array<FormControlElement>} The array of autofillable elements.
 */
__gCrWeb.autofill.extractAutofillableElementsFromSet = function(
    controlElements) {
  const autofillableElements = [];
  for (let i = 0; i < controlElements.length; ++i) {
    const element = controlElements[i];
    if (!__gCrWeb.fill.isAutofillableElement(element)) {
      continue;
    }
    autofillableElements.push(element);
  }
  return autofillableElements;
};

/**
 * Returns all the auto-fillable form control elements in |formElement|.
 *
 * It is based on the logic in
 *     void ExtractAutofillableElementsInForm(
 *         const blink::WebFormElement& form_element);
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {HTMLFormElement} formElement A form element to be processed.
 * @return {Array<FormControlElement>} The array of autofillable elements.
 */
__gCrWeb.autofill.extractAutofillableElementsInForm = function(formElement) {
  const controlElements = __gCrWeb.form.getFormControlElements(formElement);
  return __gCrWeb.autofill.extractAutofillableElementsFromSet(controlElements);
};

/**
 * For debugging purposes, annotate forms on the page with prediction data using
 * the placeholder attribute.
 *
 * @param {Object<AutofillFormData>} data The form and field identifiers with
 *     their prediction data.
 */
__gCrWeb.autofill['fillPredictionData'] = function(data) {
  for (const formName in data) {
    const form = __gCrWeb.form.getFormElementFromIdentifier(formName);
    const formData = data[formName];
    const controlElements = __gCrWeb.form.getFormControlElements(form);
    for (let i = 0; i < controlElements.length; ++i) {
      const element = controlElements[i];
      if (!__gCrWeb.fill.isAutofillableElement(element)) {
        continue;
      }
      const elementName = __gCrWeb.form.getFieldIdentifier(element);
      const value = formData[elementName];
      if (value) {
        element.placeholder = value;
      }
    }
  }
};

/**
 * Returns whether |value| contains only formating characters.
 *
 * It is based on the logic in
 *     void SanitizedFieldIsEmpty(const base::string16& value);
 * in chromium/src/components/autofill/common/autofill_util.h.
 *
 * @param {HTMLFormElement} formElement A form element to be processed.
 * @return {Array<FormControlElement>} The array of autofillable elements.
 */
__gCrWeb.autofill['sanitizedFieldIsEmpty'] = function(value) {
  // Some sites enter values such as ____-____-____-____ or (___)-___-____ in
  // their fields. Check if the field value is empty after the removal of the
  // formatting characters.
  return __gCrWeb.common.trim(value.replace(/[-_()/|]/g, '')) == '';
};

}());  // End of anonymous object
