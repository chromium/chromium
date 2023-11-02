// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var cwsContainerMock = {};
(function() {
  // This ID will be checked in the test.
  var DUMMY_ITEM_ID = 'DUMMY_ITEM_ID_FOR_TEST';

  var origin = null;
  var data = null;
  var source = null;

  cwsContainerMock.onMessage = function (message) {
    data = message.data;
    source = message.source;
    origin = message.origin;

    switch (data['message']) {
      case 'initialize':
        cwsContainerMock.onInitialize();
        break;
      case 'install_success':
        cwsContainerMock.onInstallSuccess();
        break;
    };
  }

  cwsContainerMock.onInitialize = function() {
    if (source && origin)
      source.postMessage({message: 'widget_loaded'}, origin);
  };

  cwsContainerMock.onInstallSuccess = function() {
    if (source && origin)
      source.postMessage({
          message: 'after_install',
          item_id: DUMMY_ITEM_ID
      }, origin);
  };

  cwsContainerMock.onInstallButton = function() {
    if (source && origin) {
      source.postMessage(
          {message: 'before_install', item_id:DUMMY_ITEM_ID}, origin);
    }
  };

})();

function onInstallButton() {
  cwsContainerMock.onInstallButton();
};

window.addEventListener('message', cwsContainerMock.onMessage);
