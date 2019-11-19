// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var certificateProviderInternal = getInternalApi('certificateProviderInternal');

var certificateProviderSchema =
    requireNative('schema_registry').GetSchema('certificateProvider')
var utils = require('utils');

// Custom bindings for chrome.certificateProvider API.
// The bindings are used to implement callbacks for the API events. Internally
// each event is passed a requestId argument used to identify the callback
// associated with the event. This argument is massaged out from the event
// arguments before dispatching the event to consumers. A callback is appended
// to the event arguments. The callback wraps an appropriate
// chrome.certificateProviderInternal API function that is used to report the
// event result from the extension. The function is passed the requestId and
// values provided by the extension. It validates that the values provided by
// the extension match chrome.certificateProvider event callback schemas. It
// also ensures that a callback is run at most once. In case there is an
// exception during event dispatching, the chrome.certificateProviderInternal
// function is called with a default error value.

// Handles a chrome.certificateProvider event as described in the file comment.
// |eventName|: The event name. The first argument of the event must be a
//     request id.
// |internalReportFunc|: The function that should be called to report results in
//     reply to an event. The first argument of the function must be the request
//     id that was received with the event.
function handleEvent(eventName, internalReportFunc) {
  var eventSchema =
      utils.lookup(certificateProviderSchema.events, 'name', eventName);
  var callbackSchema =
      utils.lookup(eventSchema.parameters, 'type', 'function').parameters;
  var fullEventName = 'certificateProvider.' + eventName;

  bindingUtil.addCustomSignature(fullEventName, callbackSchema);

  bindingUtil.registerEventArgumentMassager(fullEventName,
                                            function(args, dispatch) {
    var responded = false;

    // Function provided to the extension as the event callback argument.
    // The extension calls this to report results in reply to the event.
    // It throws an exception if called more than once and if the provided
    // results don't match the callback schema.
    var reportFunc = function(reportArg1, reportArg2) {
      if (responded)
        throw new Error('Event callback must not be called more than once.');

      var reportArgs = [reportArg1];
      if (reportArg2 !== undefined)
        reportArgs.push(reportArg2);
      var finalArgs = [];
      try {
        // Validates that the results reported by the extension matche the
        // callback schema of the event. Throws an exception in case of an
        // error.
        bindingUtil.validateCustomSignature(fullEventName, reportArgs);
        finalArgs = reportArgs;
      } finally {
        responded = true;
        internalReportFunc.apply(
            null, [args[0] /* requestId */].concat(finalArgs));
      }
    };
    dispatch(args.slice(1).concat(reportFunc));
  });
}

handleEvent('onCertificatesRequested',
            certificateProviderInternal.reportCertificates);

handleEvent('onSignDigestRequested',
            certificateProviderInternal.reportSignature);
