// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Contains method needed to access the forms and their elements.
 */

goog.provide('__crWeb.form');

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected. String 'form' is used in |__gCrWeb['form']| as it needs to be
 * accessed in Objective-C code.
 */
__gCrWeb.form = {};

// Store common namespace object in a global __gCrWeb object referenced by a
// string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['form'] = __gCrWeb.form;

/** Beginning of anonymous object */
(function() {
/**
 * Prefix used in references to form elements that have no 'id' or 'name'
 */
__gCrWeb.form.kNamelessFormIDPrefix = 'gChrome~form~';

/**
 * Prefix used in references to field elements that have no 'id' or 'name' but
 * are included in a form.
 */
__gCrWeb.form.kNamelessFieldIDPrefix = 'gChrome~field~';

/**
 * A WeakMap to track if the current value of a field was entered by user or
 * programmatically.
 * If the map is null, the source of changed is not track.
 */
__gCrWeb.form.wasEditedByUser = null;

/**
 * Based on Element::isFormControlElement() (WebKit)
 * @param {Element} element A DOM element.
 * @return {boolean} true if the |element| is a form control element.
 */
__gCrWeb.form.isFormControlElement = function(element) {
  const tagName = element.tagName;
  return (
      tagName === 'INPUT' || tagName === 'SELECT' || tagName === 'TEXTAREA');
};

/**
 * Returns an array of control elements in a form.
 *
 * This method is based on the logic in method
 *     void WebFormElement::getFormControlElements(
 *         WebVector<WebFormControlElement>&) const
 * in chromium/src/third_party/WebKit/Source/WebKit/chromium/src/
 * WebFormElement.cpp.
 *
 * @param {Element} form A form element for which the control elements are
 *   returned.
 * @return {Array<FormControlElement>}
 */
__gCrWeb.form.getFormControlElements = function(form) {
  if (!form) {
    return [];
  }
  const results = [];
  // Get input and select elements from form.elements.
  // According to
  // http://www.w3.org/TR/2011/WD-html5-20110525/forms.html, form.elements are
  // the "listed elements whose form owner is the form element, with the
  // exception of input elements whose type attribute is in the Image Button
  // state, which must, for historical reasons, be excluded from this
  // particular collection." In WebFormElement.cpp, this method is implemented
  // by returning elements in form's associated elements that have tag 'INPUT'
  // or 'SELECT'. Check if input Image Buttons are excluded in that
  // implementation. Note for Autofill, as input Image Button is not
  // considered as autofillable elements, there is no impact on Autofill
  // feature.
  const elements = form.elements;
  for (let i = 0; i < elements.length; i++) {
    if (__gCrWeb.form.isFormControlElement(elements[i])) {
      results.push(/** @type {FormControlElement} */ (elements[i]));
    }
  }
  return results;
};

/**
 * Returns the field's |id| attribute if not space only; otherwise the
 * form's |name| attribute if the field is part of a form. Otherwhise,
 * generate a technical identifier
 *
 * It is the identifier that should be used for the specified |element| when
 * storing Autofill data. This identifier will be used when filling the field
 * to lookup this field. The pair (getFormIdentifier, getFieldIdentifier) must
 * be unique on the page.
 * The following elements are considered to generate the identifier:
 * - the id of the element
 * - the name of the element if the element is part of a form
 * - the order of the element in the form if the element is part of the form.
 * - generate a xpath to the element and use it as an ID.
 *
 * Note: if this method returns '', the field will not be accessible and
 * cannot be autofilled.
 *
 * It aims to provide the logic in
 *     WebString nameForAutofill() const;
 * in chromium/src/third_party/WebKit/Source/WebKit/chromium/public/
 *  WebFormControlElement.h
 *
 * @param {Element} element An element of which the name for Autofill will be
 *     returned.
 * @return {string} the name for Autofill.
 */
__gCrWeb.form.getFieldIdentifier = function(element) {
  if (!element) {
    return '';
  }
  let trimmedIdentifier = element.id;
  if (trimmedIdentifier) {
    return __gCrWeb.common.trim(trimmedIdentifier);
  }
  if (element.form) {
    // The name of an element is only relevant as an identifier if the element
    // is part of a form.
    trimmedIdentifier = element.name;
    if (trimmedIdentifier) {
      trimmedIdentifier = __gCrWeb.common.trim(trimmedIdentifier);
      if (trimmedIdentifier.length > 0) {
        return trimmedIdentifier;
      }
    }

    const elements = __gCrWeb.form.getFormControlElements(element.form);
    for (let index = 0; index < elements.length; index++) {
      if (elements[index] === element) {
        return __gCrWeb.form.kNamelessFieldIDPrefix + index;
      }
    }
  }
  // Element is not part of a form and has no name or id, or usable attribute.
  // As best effort, try to find the closest ancestor with an id, then
  // check the index of the element in the descendants of the ancestors with
  // the same type.
  let ancestor = element.parentNode;
  while (!!ancestor && ancestor.nodeType == Node.ELEMENT_NODE &&
         (!ancestor.hasAttribute('id') ||
          __gCrWeb.common.trim(ancestor.id) == '')) {
    ancestor = ancestor.parentNode;
  }
  const query = element.tagName;
  let ancestorId = '';
  if (!ancestor || ancestor.nodeType != Node.ELEMENT_NODE) {
    ancestor = document.body;
  }
  if (ancestor.hasAttribute('id')) {
    ancestorId = '#' + __gCrWeb.common.trim(ancestor.id);
  }
  const descendants = ancestor.querySelectorAll(element.tagName);
  let i = 0;
  for (i = 0; i < descendants.length; i++) {
    if (descendants[i] === element) {
      return __gCrWeb.form.kNamelessFieldIDPrefix + ancestorId + '~' +
          element.tagName + '~' + i;
    }
  }

  return '';
};

/**
 * Returns the field's |name| attribute if not space only; otherwise the
 * field's |id| attribute.
 *
 * The name will be used as a hint to infer the autofill type of the field.
 *
 * It aims to provide the logic in
 *     WebString nameForAutofill() const;
 * in chromium/src/third_party/WebKit/Source/WebKit/chromium/public/
 *  WebFormControlElement.h
 *
 * @param {Element} element An element of which the name for Autofill will be
 *     returned.
 * @return {string} the name for Autofill.
 */
__gCrWeb.form.getFieldName = function(element) {
  if (!element) {
    return '';
  }
  let trimmedName = element.name;
  if (trimmedName) {
    trimmedName = __gCrWeb.common.trim(trimmedName);
    if (trimmedName.length > 0) {
      return trimmedName;
    }
  }
  trimmedName = element.id;
  if (trimmedName) {
    return __gCrWeb.common.trim(trimmedName);
  }
  return '';
};

/**
 * Returns the form's |name| attribute if non-empty; otherwise the form's |id|
 * attribute, or the index of the form (with prefix) in document.forms.
 *
 * It is partially based on the logic in
 *     const string16 GetFormIdentifier(const blink::WebFormElement& form)
 * in chromium/src/components/autofill/renderer/form_autofill_util.h.
 *
 * @param {Element} form An element for which the identifier is returned.
 * @return {string} a string that represents the element's identifier.
 */
__gCrWeb.form.getFormIdentifier = function(form) {
  if (!form) return '';
  let name = form.getAttribute('name');
  if (name && name.length != 0 &&
      form.ownerDocument.forms.namedItem(name) === form) {
    return name;
  }
  name = form.getAttribute('id');
  if (name && name.length != 0 &&
      form.ownerDocument.getElementById(name) === form) {
    return name;
  }
  // A form name must be supplied, because the element will later need to be
  // identified from the name. A last resort is to take the index number of
  // the form in document.forms. ids are not supposed to begin with digits (by
  // HTML 4 spec) so this is unlikely to match a true id.
  for (let idx = 0; idx != document.forms.length; idx++) {
    if (document.forms[idx] == form) {
      return __gCrWeb.form.kNamelessFormIDPrefix + idx;
    }
  }
  return '';
};

/**
 * Returns the form element from an ID obtained from getFormIdentifier.
 *
 * This works on a 'best effort' basis since DOM changes can always change the
 * actual element that the ID refers to.
 *
 * @param {string} name An ID string obtained via getFormIdentifier.
 * @return {HTMLFormElement} The original form element, if it can be determined.
 */
__gCrWeb.form.getFormElementFromIdentifier = function(name) {
  // First attempt is from the name / id supplied.
  const form = document.forms.namedItem(name);
  if (form) {
    if (form.nodeType !== Node.ELEMENT_NODE) return null;
    return (form);
  }
  // Second attempt is from the prefixed index position of the form in
  // document.forms.
  if (name.indexOf(__gCrWeb.form.kNamelessFormIDPrefix) == 0) {
    const nameAsInteger =
        0 | name.substring(__gCrWeb.form.kNamelessFormIDPrefix.length);
    if (__gCrWeb.form.kNamelessFormIDPrefix + nameAsInteger == name &&
        nameAsInteger < document.forms.length) {
      return document.forms[nameAsInteger];
    }
  }
  return null;
};

/**
 * Returns whether the last |input| or |change| event on |element| was
 * triggered by a user action (was "trusted").
 */
__gCrWeb.form['fieldWasEditedByUser'] = function(element) {
  if (__gCrWeb.form.wasEditedByUser === null) {
    // Input event sources is not tracked.
    // Return true to preserve previous behavior.
    return true;
  }
  if (!__gCrWeb.form.wasEditedByUser.has(element)) {
    return false;
  }
  return __gCrWeb.form.wasEditedByUser.get(element);
};

}());  // End of anonymous object
