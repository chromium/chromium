// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Prefix used in references to form elements that have no 'id' or 'name'
 */
const kNamelessFormIDPrefix = 'gChrome~form~';

/**
 * Prefix used in references to field elements that have no 'id' or 'name' but
 * are included in a form.
 */
const kNamelessFieldIDPrefix = 'gChrome~field~';


/**
 * Based on Element::isFormControlElement() (WebKit)
 * @param element A DOM element.
 * @return true if the `element` is a form control element.
 */
export function isFormControlElement(element: Element): boolean {
  const tagName = element.tagName;
  return (
      tagName === 'INPUT' || tagName === 'SELECT' || tagName === 'TEXTAREA');
}

/**
 * Returns the form's `name` attribute if non-empty; otherwise the form's `id`
 * attribute, or the index of the form (with prefix) in document.forms.
 *
 * It is partially based on the logic in
 *     const string16 GetFormIdentifier(const blink::WebFormElement& form)
 * in chromium/src/components/autofill/renderer/form_autofill_util.h.
 *
 * @param form An element for which the identifier is returned.
 * @return a string that represents the element's identifier.
 */
export function getFormIdentifier(form: Element|null): string {
  if (!form) {
    return '';
  }

  let name = form.getAttribute('name');
  if (name && name.length !== 0 &&
      form.ownerDocument.forms.namedItem(name) === form) {
    return name;
  }
  name = form.getAttribute('id');
  if (name && name.length !== 0 &&
      form.ownerDocument.getElementById(name) === form) {
    return name;
  }
  // A form name must be supplied, because the element will later need to be
  // identified from the name. A last resort is to take the index number of
  // the form in document.forms. ids are not supposed to begin with digits (by
  // HTML 4 spec) so this is unlikely to match a true id.
  for (let idx = 0; idx !== document.forms.length; idx++) {
    if (document.forms[idx] === form) {
      return kNamelessFormIDPrefix + idx;
    }
  }
  return '';
}

/**
 * Returns the form element from an ID obtained from getFormIdentifier.
 *
 * This works on a 'best effort' basis since DOM changes can always change the
 * actual element that the ID refers to.
 *
 * @param name An ID string obtained via getFormIdentifier.
 * @return The original form element, if it can be determined.
 */
export function getFormElementFromIdentifier(name: string): HTMLFormElement|
    null {
  // First attempt is from the name / id supplied.
  const form = document.forms.namedItem(name);
  if (form) {
    return form.nodeType === Node.ELEMENT_NODE ? form : null;
  }
  // Second attempt is from the prefixed index position of the form in
  // document.forms.
  if (name.indexOf(kNamelessFormIDPrefix) === 0) {
    const nameAsInteger =
        0 | name.substring(kNamelessFieldIDPrefix.length).length;
    if (kNamelessFormIDPrefix + nameAsInteger === name &&
        nameAsInteger < document.forms.length) {
      const form = document.forms[nameAsInteger];
      return form ? form : null;
    }
  }
  return null;
}

/**
 * Returns an array of control elements in a form.
 *
 * This method is based on the logic in method
 *     void WebFormElement::getFormControlElements(
 *         WebVector<WebFormControlElement>&) const
 * in chromium/src/third_party/WebKit/Source/WebKit/chromium/src/
 * WebFormElement.cpp.
 *
 * @param form A form element for which the control elements are returned.
 * @return An array of form control elements.
 */
export function getFormControlElements(form: HTMLFormElement|null): Element[] {
  if (!form) {
    return [];
  }
  const results: Element[] = [];
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
  for (const element of form.elements) {
    if (isFormControlElement(element)) {
      results.push(element);
    }
  }
  return results;
}
