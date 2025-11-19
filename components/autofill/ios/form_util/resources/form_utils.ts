// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Prefix used in references to form elements that have no 'id' or 'name'
 */
const kNamelessFormIDPrefix = 'gChrome~form~';

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
