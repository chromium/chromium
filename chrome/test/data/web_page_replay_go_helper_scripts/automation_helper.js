// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const automation_helper = (function() {
  var automation_helper = {
    // An enum specifying the state of an element on the page.
    DomElementReadyState:
      Object.freeze({
          "present": 0,
          "visible": 1,
          "enabled": 2,
          "on_top": 4,
      }),
  };

 // public:
  // Checks if an element is present, visible or enabled on the page.
  automation_helper.isElementReady = function(
      getElementFunction,
      state_flags =
          this.DomElementReadyState.visible |
          this.DomElementReadyState.enabled |
          this.DomElementReadyState.on_top) {
    const element = getElementFunction();
    if (element) {
      var isReady = true;

      if (state_flags & this.DomElementReadyState.visible) {
        var target = element;
        // In some custom select drop downs, like the ones on Amazon.com and
        // Zappos.com, the drop down options are hosted inside a span element
        // that is the immediate sibling, rather than the descendant, of the
        // select dropdown.
        // In these cases, check if the span is visible instead.
        if (element.offsetParent === null &&
            element instanceof HTMLSelectElement &&
            element.nextElementSibling instanceof HTMLSpanElement)
          target = element.nextElementSibling;

        isReady =
            (target.offsetParent !== null) &&
            (target.offsetWidth > 0) &&
            (target.offsetHeight > 0);

        if (isReady && state_flags & this.DomElementReadyState.on_top) {
          var rect = target.getBoundingClientRect();
          // Check that the element is not concealed behind another element.
          const topElement = document.elementFromPoint(
              // As coordinates, use the center of the element, minus the
              // window offset in case the element is outside the view.
              rect.left + rect.width / 2 - window.pageXOffset,
              rect.top + rect.height / 2 - window.pageYOffset);
          isReady &= target.contains(topElement) ||
                     target.isSameNode(topElement);
        }
      }

      if (isReady && state_flags & this.DomElementReadyState.enabled) {
        isReady = !element.disabled;
      }

      return isReady;
    }
    return false;
  };

  // Check if an element identified by a xpath is present, visible or
  // enabled on the page.
  automation_helper.isElementWithXpathReady = function(
      xpath,
      state_flags = this.DomElementReadyState.visible
                  | this.DomElementReadyState.enabled) {
    return this.isElementReady(
      function(){
        return automation_helper.getElementByXpath(xpath);
      },
      state_flags);
  };

  // Simulates the user selecting a dropdown option by setting the dropdown
  // option and then fire an onchange event on the dropdown element.
  automation_helper.selectOptionFromDropDownElementByIndex =
    function (dropdown, index) {
    dropdown.options.selectedIndex = index;
    triggerOnChangeEventOnElement(dropdown);
  };

  // Simulates the user interacting with an input element by setting the input
  // value and then fire
  // an onchange event on the input element.
  automation_helper.setInputElementValue = function (element, value) {
    element.value = value;
    triggerOnChangeEventOnElement(element);
  };

  automation_helper.getElementByXpath = function (path) {
    return document.evaluate(path, document, null,
        XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;
  };

 // private:
  // Triggers an onchange event on a dom element.
  function triggerOnChangeEventOnElement(element) {
    var event = document.createEvent('HTMLEvents');
    event.initEvent('change', false, true);
    element.dispatchEvent(event);
  }

  return automation_helper;
})();
