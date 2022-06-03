// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the GCM API.

var forEach = require('utils').forEach;

apiBridge.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;
  var gcm = bindingsAPI.compiledApi;

  apiFunctions.setUpdateArgumentsPostValidate(
    'send', function(message, callback) {
      // Validate message.data.
      var payloadSize = 0;
      forEach(message.data, function(property, value) {
        if (property.length === 0)
          throw new Error("One of data keys is empty.");

        var lowerCasedProperty = property.toLowerCase();
        // Issue an error for forbidden prefixes of property names.
        if (lowerCasedProperty.startsWith("goog.") ||
            lowerCasedProperty.startsWith("google") ||
            property.startsWith("collapse_key")) {
          throw new Error("Invalid data key: " + property);
        }

        payloadSize += property.length + value.length;
      });

      if (payloadSize > gcm.MAX_MESSAGE_SIZE)
        throw new Error("Payload exceeded allowed size limit. Payload size is: "
            + payloadSize);

      if (payloadSize === 0)
        throw new Error("No data to send.");

      return arguments;
    });
});
