// Copyright 2018 The Chromium Authors
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
      xpath = "",
      state_flags =
          this.DomElementReadyState.visible |
          this.DomElementReadyState.enabled |
          this.DomElementReadyState.on_top) {
    let isReady = true;
    // Some sites override the console function locally,
    // this ensures our function can write to log
    var frame = document.createElement('iframe');
    try {
      document.body.appendChild(frame);
      console = frame.contentWindow.console;

      const element = getElementFunction();
      let logDataArr = [];
      logDataArr.push('[Element (' + xpath + ')]');
      if (element) {
        logDataArr.push('[FOUND]');
        let target = element;
        if (state_flags & this.DomElementReadyState.visible) {
          // In some custom select drop downs, like the ones on Amazon.com and
          // Zappos.com, the drop down options are hosted inside a span element
          // that is the immediate sibling, rather than the descendant, of the
          // select dropdown.
          // In these cases, check if the span is visible instead.
          if (element.offsetParent === null &&
              element instanceof HTMLSelectElement &&
              element.nextElementSibling instanceof HTMLSpanElement) {
            logDataArr.push("[Moved to nextElementSibling]");
            target = element.nextElementSibling;
          }
          const isVisible = (target.offsetParent !== null) &&
            (target.offsetWidth > 0) && (target.offsetHeight > 0);
          logDataArr.push('[isVisible:' + isVisible + ']');
          isReady = isReady && isVisible;
        }
        if (state_flags & this.DomElementReadyState.on_top) {
          // The document.elementFromPoint function only acts on an element
          // inside the viewport. Actively scroll the element into view first.
          element.scrollIntoView({block:"center", inline:"center"});
          const rect = target.getBoundingClientRect();
          // Check that the element is not concealed behind another element.
          const topElement = document.elementFromPoint(
              // As coordinates, use the center of the element, minus the
              // window offset in case the element is outside the view.
              rect.left + rect.width / 2, rect.top + rect.height / 2);
          const isTop = target.contains(topElement) ||
                        target.isSameNode(topElement);
          isReady = isReady && isTop;
          logDataArr.push('[OnTop:' + isTop + ':' + topElement.localName + ']');
        }
        if (state_flags & this.DomElementReadyState.enabled) {
          const isEnabled = !element.disabled;
          logDataArr.push('[Enabled:' + isEnabled + ']');
          isReady = isReady && isEnabled;
        }
      } else {
        isReady = false;
        logDataArr.push('[NOT FOUND]');
      }
      logDataArr.push('[FinalReady:' + isReady + ']');
      console.log(logDataArr.join(""));
    } finally {
      // Remove our temporary console iframe
      if(document.body.contains(frame))
        document.body.removeChild(frame);
    }
    return isReady;
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
      xpath,
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
(function () {
  // Some sites have beforeunload triggers to stop user navigation away.
  // For testing purposes, we can suppress those here.
  window.addEventListener('beforeunload', function (event) {
    event.stopImmediatePropagation();
  });
})();
// Some sites rely on Math.random returning specific values. Changing the
// behavior of this function can break some of
// AutofillCapturedSitesInteractiveTest tests.
(function() {
var random_count = 0;
var random_count_threshold = 25;
var random_seed = 0.462;
Math.random = function() {
  random_count++;
  if (random_count > random_count_threshold) {
    random_seed += 0.1;
    random_count = 1;
  }
  return (random_seed % 1);
};
if (typeof (crypto) == 'object' &&
    typeof (crypto.getRandomValues) == 'function') {
  crypto.getRandomValues = function(arr) {
    var scale = Math.pow(256, arr.BYTES_PER_ELEMENT);
    for (var i = 0; i < arr.length; i++) {
      arr[i] = Math.floor(Math.random() * scale);
    }
    return arr;
  };
}
})();
// Some sites rely on Date returning specific values. Changing the behavior of
// this function can break some of AutofillCapturedSitesInteractiveTest tests.
(function() {
var date_count = 0;
var date_count_threshold = 25;
var orig_date = Date;
// Time since epoch in milliseconds. This is replaced by script injector with
// the date when the recording is done.
var time_seed = {{WPR_TIME_SEED_TIMESTAMP}};
Date = function() {
  if (this instanceof Date) {
    date_count++;
    if (date_count > date_count_threshold) {
      time_seed += 50;
      date_count = 1;
    }
    switch (arguments.length) {
      case 0:
        return new orig_date(time_seed);
      case 1:
        return new orig_date(arguments[0]);
      default:
        return new orig_date(
            arguments[0], arguments[1],
            arguments.length >= 3 ? arguments[2] : 1,
            arguments.length >= 4 ? arguments[3] : 0,
            arguments.length >= 5 ? arguments[4] : 0,
            arguments.length >= 6 ? arguments[5] : 0,
            arguments.length >= 7 ? arguments[6] : 0);
    }
  }
  return new Date().toString();
};
Date.__proto__ = orig_date;
Date.prototype = orig_date.prototype;
Date.prototype.constructor = Date;
orig_date.now = function() {
  return new Date().getTime();
};
orig_date.prototype.getTimezoneOffset = function() {
  var dst2010Start = 1268560800000;
  var dst2010End = 1289120400000;
  if (this.getTime() >= dst2010Start && this.getTime() < dst2010End)
    return 420;
  return 480;
};
})();
