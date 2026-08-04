// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function focus(element, focusDocumentElement) {
  // Focus the target element in order to send keys to it.
  // First, the currently active element is blurred, if it is different from
  // the target element. We do not want to blur an element unnecessarily,
  // because this may cause us to lose the current cursor position in the
  // element.
  // Secondly, we focus the target element.
  // Thirdly, we check if the new active element is the target element. If not,
  // we throw an error.
  // Additional notes:
  //   - |document.activeElement| is the currently focused element, or body if
  //     no element is focused
  //   - Even if |document.hasFocus()| returns true and the active element is
  //     the body, sometimes we still need to focus the body element for send
  //     keys to work. Not sure why
  //   - You cannot focus a descendant of a content editable node
  //   - V8 throws a TypeError when calling setSelectionRange for a non-text
  //     input, which still have setSelectionRange defined. For chrome 29+, V8
  //     throws a DOMException with code InvalidStateError.
  var doc = element.ownerDocument || element;
  var prevActiveElement = doc.activeElement;
  if (element != prevActiveElement && prevActiveElement)
    prevActiveElement.blur();
  element.focus();

  var activeElement = doc.activeElement;

  // Off-screen elements may fail focus() because the browser cannot scroll
  // them into view. Retry with preventScroll before giving up.
  if (element != activeElement && !element.contains(activeElement)) {
    element.focus({preventScroll: true});
    activeElement = doc.activeElement;
  }
  // If the element is in a shadow DOM, then as far as the document is
  // concerned, the shadow host is the active element. We need to go through the
  // tree of shadow DOMs to check that the element we gave focus to is now
  // active.
  if (element != activeElement && !element.contains(activeElement)) {
    var shadowRoot = activeElement.shadowRoot;
    while (shadowRoot) {
      var activeElement = shadowRoot.activeElement;
      if (element == activeElement) {
        // the shadow DOM's active element is our requested element. We're good.
        break;
      }
      // The shadow DOM's active element isn't our requested element, check to
      // see if there's a nested shadow DOM.
      shadowRoot = activeElement.shadowRoot;
    }
  }
  if (element != activeElement && !element.contains(activeElement))
    throw new Error('cannot focus element');
  // On Android, focus() on a non-focusable container (e.g. <html>) can redirect
  // to a descendant. tabindex=-1 makes it focusable; kept intentionally since
  // removing it resets document.activeElement back to the descendant.
  if (focusDocumentElement && element === doc.documentElement &&
      element != activeElement && element.contains(activeElement)) {
    if (!element.hasAttribute('tabindex'))
      element.setAttribute('tabindex', '-1');
    element.focus({preventScroll: true});
    if (doc.activeElement !== element)
      throw new Error('cannot focus element');
  }
}
