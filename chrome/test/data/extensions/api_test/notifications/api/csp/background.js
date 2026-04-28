// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const notifications = chrome.notifications;

const idString = 'foo';

const testCSP = function() {
  const onCreateCallback = function(id) {
    if (chrome.runtime.lastError) {
      chrome.test.succeed();
      return;
    }
    chrome.test.fail();
  };

  const options = {
    type: 'basic',
    iconUrl: 'http://google.com/clearly-a-security-problem.png',
    title: 'Attention!',
    message: 'Check out Cirque du Soleil',
  };
  notifications.create(idString, options, onCreateCallback);
};

function testDataURL() {
  chrome.runtime.lastError = undefined;
  const onCreateCallback = function(id) {
    if (chrome.runtime.lastError) {
      chrome.test.fail();
      return;
    }
    chrome.test.succeed();
  };
  const options = {
    type: 'basic',
    iconUrl: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAA' +
        'CNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHw' +
        'AAAABJRU5ErkJggg==',
    title: 'Attention!',
    message: 'Check out Cirque du Soleil',
  };
  notifications.create(idString, options, onCreateCallback);
}

function testCSPUpdateIconURL() {
  const onUpdateCallback = function(id) {
    if (chrome.runtime.lastError) {
      chrome.test.succeed();
      return;
    }
    chrome.test.fail();
  };
  const onCreateCallback = function(id) {
    if (chrome.runtime.lastError) {
      chrome.test.fail();
      return;
    }
    const options2 = {
      type: 'basic',
      iconUrl: 'http://www.google.com/favicon.ico',
      title: 'Attention!',
      message: 'Check out Cirque du Soleil',
    };
    notifications.update(idString, options2, onUpdateCallback);
  };
  const options = {
    type: 'basic',
    iconUrl: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAA' +
        'CNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHw' +
        'AAAABJRU5ErkJggg==',
    title: 'Attention!',
    message: 'Check out Cirque du Soleil',
  };
  notifications.create(idString, options, onCreateCallback);
}

function testCSPUpdateImageURL() {
  const onUpdateCallback = function(id) {
    if (chrome.runtime.lastError) {
      chrome.test.succeed();
      return;
    }
    chrome.test.fail();
  };
  const onCreateCallback = function(id) {
    if (chrome.runtime.lastError) {
      chrome.test.fail();
      return;
    }
    const options2 = {
      type: 'basic',
      iconUrl: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAA' +
          'CNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHw' +
          'AAAABJRU5ErkJggg==',
      imageUrl: 'http://www.google.com/favicon.ico',
      title: 'Attention!',
      message: 'Check out Cirque du Soleil',
    };
    notifications.update(idString, options2, onUpdateCallback);
  };
  const options = {
    type: 'image',
    iconUrl: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAA' +
        'CNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHw' +
        'AAAABJRU5ErkJggg==',
    imageUrl: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAA' +
        'CNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHw' +
        'AAAABJRU5ErkJggg==',
    title: 'Attention!',
    message: 'Check out Cirque du Soleil',
  };
  notifications.create(idString, options, onCreateCallback);
}

function testCSPUpdateButtonIconURL() {
  const onUpdateCallback = function(id) {
    if (chrome.runtime.lastError) {
      chrome.test.succeed();
      return;
    }
    chrome.test.fail();
  };
  const onCreateCallback = function(id) {
    if (chrome.runtime.lastError) {
      chrome.test.fail();
      return;
    }
    const options2 = {
      type: 'basic',
      iconUrl: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAA' +
          'CNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHw' +
          'AAAABJRU5ErkJggg==',
      title: 'Attention!',
      buttons: [{
        title: 'Foo',
        iconUrl: 'http://www.google.com/favicon.ico',
      }],
      message: 'Check out Cirque du Soleil',
    };
    notifications.update(idString, options2, onUpdateCallback);
  };
  const options = {
    type: 'basic',
    iconUrl: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAA' +
        'CNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHw' +
        'AAAABJRU5ErkJggg==',
    title: 'Attention!',
    buttons: [{
      title: 'Foo',
      iconUrl: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAA' +
          'CNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHw' +
          'AAAABJRU5ErkJggg==',
    }],
    message: 'Check out Cirque du Soleil',
  };
  notifications.create(idString, options, onCreateCallback);
}
chrome.test.runTests([
  testCSP,
  testDataURL,
  testCSPUpdateIconURL,
  testCSPUpdateImageURL,
  testCSPUpdateButtonIconURL,
]);
