// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the notifications API.
//
var exceptionHandler = require('uncaught_exception_handler');
var imageUtil = require('imageUtil');
var notificationsPrivate = requireNative('notifications_private');

function imageDataSetter(context, key) {
  var f = function(val) {
    this[key] = val;
  };
  return $Function.bind(f, context);
}

// A URL Spec is an object with the following keys:
//  path: The resource to be downloaded.
//  width: (optional) The maximum width of the image to be downloaded in device
//         pixels.
//  height: (optional) The maximum height of the image to be downloaded in
//          device pixels.
//  callback: A function to be called when the URL is complete. It
//    should accept an ImageData object and set the appropriate
//    field in |notificationDetails|.
function getUrlSpecs(imageSizes, notificationDetails) {
  var urlSpecs = [];

  // |iconUrl| might be optional for notification updates.
  if (notificationDetails.iconUrl) {
    $Array.push(urlSpecs, {
      path: notificationDetails.iconUrl,
      width: imageSizes.icon.width * imageSizes.scaleFactor,
      height: imageSizes.icon.height * imageSizes.scaleFactor,
      callback: imageDataSetter(notificationDetails, 'iconBitmap')
    });
  }

  // |appIconMaskUrl| is optional.
  if (notificationDetails.appIconMaskUrl) {
    $Array.push(urlSpecs, {
      path: notificationDetails.appIconMaskUrl,
      width: imageSizes.appIconMask.width * imageSizes.scaleFactor,
      height: imageSizes.appIconMask.height * imageSizes.scaleFactor,
      callback: imageDataSetter(notificationDetails, 'appIconMaskBitmap')
    });
  }

  // |imageUrl| is optional.
  if (notificationDetails.imageUrl) {
    $Array.push(urlSpecs, {
      path: notificationDetails.imageUrl,
      width: imageSizes.image.width * imageSizes.scaleFactor,
      height: imageSizes.image.height * imageSizes.scaleFactor,
      callback: imageDataSetter(notificationDetails, 'imageBitmap')
    });
  }

  // Each button has an optional icon.
  var buttonList = notificationDetails.buttons;
  if (buttonList && typeof buttonList.length === 'number') {
    var numButtons = buttonList.length;
    for (var i = 0; i < numButtons; i++) {
      if (buttonList[i].iconUrl) {
        $Array.push(urlSpecs, {
          path: buttonList[i].iconUrl,
          width: imageSizes.buttonIcon.width * imageSizes.scaleFactor,
          height: imageSizes.buttonIcon.height * imageSizes.scaleFactor,
          callback: imageDataSetter(buttonList[i], 'iconBitmap')
        });
      }
    }
  }

  return urlSpecs;
}

function replaceNotificationOptionURLs(notification_details, callback) {
  var imageSizes = notificationsPrivate.GetNotificationImageSizes();
  var url_specs = getUrlSpecs(imageSizes, notification_details);
  if (!url_specs.length) {
    callback(true);
    return;
  }

  var errors = 0;

  imageUtil.loadAllImages(url_specs, {
    onerror: function(index) {
      errors++;
    },
    oncomplete: function(imageData) {
      if (errors > 0) {
        callback(false);
        return;
      }
      for (var index = 0; index < url_specs.length; index++) {
        var url_spec = url_specs[index];
        url_spec.callback(imageData[index]);
      }
      callback(true);
    }
  });
}

function genHandle(name, failure_function) {
  return function(id, input_notification_details, callback) {
    // TODO(dewittj): Remove this hack. This is used as a way to deep
    // copy a complex JSON object.
    var notification_details = $JSON.parse(
        $JSON.stringify(input_notification_details));
    var that = this;
    var stack = exceptionHandler.getExtensionStackTrace();
    replaceNotificationOptionURLs(notification_details, function(success) {
      if (success) {
        bindingUtil.sendRequest(
            name, [id, notification_details, callback], undefined);
        return;
      }
      bindingUtil.runCallbackWithLastError(
          'Unable to download all specified images.',
          $Function.bind(failure_function, null,
                         callback || function() {}, id));
    });
  };
}

var handleCreate = genHandle('notifications.create',
                             function(callback, id) { callback(id); });
var handleUpdate = genHandle('notifications.update',
                             function(callback, id) { callback(false); });

var notificationsCustomHook = function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;
  apiFunctions.setHandleRequest('create', handleCreate);
  apiFunctions.setHandleRequest('update', handleUpdate);
};

apiBridge.registerCustomHook(notificationsCustomHook);
