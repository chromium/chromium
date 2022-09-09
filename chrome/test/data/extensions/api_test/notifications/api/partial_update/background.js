// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const notifications = chrome.notifications;

function arrayEquals(a, b) {
  if (a === b) return true;
  if (a == null || b == null) return false;
  if (a.length !== b.length) return false;

  for (var i = 0; i < a.length; i++) {
    if (a[i] !== b[i]) return false;
  }
  return true;
};

function create(id, options) {
  return new Promise(function (resolve, reject) {
    notifications.create(id, options, function (id) {
      if (chrome.runtime.lastError) {
        reject(new Error("Unable to create notification"));
        return;
      }
      console.log("Created with id: " + id);
      resolve(id);
      return;
    });
  });
};

function update(id, options) {
  return new Promise(function (resolve, reject) {
    notifications.update(id, options, function (ok) {
      if (chrome.runtime.lastError || !ok) {
        reject(new Error("Unable to update notification"));
        return;
      }
      console.log("Updated id: ", id);
      resolve(ok);
      return;
    });
  });
}

function clear(id) {
  return new Promise(function (resolve, reject) {
    notifications.clear(id, function (ok) {
      if (chrome.runtime.lastError || !ok) {
        reject(new Error("Unable to clear notification"));
        return;
      }
      resolve(ok);
      return;
    });
  });
}

function getAll() {
  return new Promise(function (resolve, reject) {
    notifications.getAll(function (ids) {
      if (chrome.runtime.lastError) {
        reject(new Error(chrome.runtime.lastError.message));
        return;
      }

      if (ids === undefined) {
        resolve([]);
        return
      }

      var id_list = Object.keys(ids);
      resolve(id_list);
    });
  });
}

function clearAll() {
  return getAll().then(function (ids) {
    var idPromises = ids.map(function (id) { return clear(id); });
    return Promise.all(idPromises);
  });
}

function succeedTest(testName) {
  return function () {
    return clearAll().then(
        function () { chrome.test.succeed(testName); },
        function (error) {
          console.log("Unknown error in clearAll: " +
              JSON.stringify(arguments));
        });
  };
}

function failTest(testName) {
  return function () {
    return clearAll().then(
        function () { chrome.test.fail(testName); },
        function (error) {
          console.log("Unknown error in clearAll: " +
              JSON.stringify(error.message));
        });
  };
}

function testPartialUpdate() {
  var testName = "testPartialUpdate";
  console.log("Starting " + testName);
  var succeed = succeedTest(testName);
  var fail = failTest(testName);

  const red_dot = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUA" +
      "AAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO" +
      "9TXL0Y4OHwAAAABJRU5ErkJggg==";

  var basicNotificationOptions = {
    type: "basic",
    title: "Basic title",
    message: "Basic message",
    iconUrl: red_dot,
    silent: false,
    buttons: [{title: "Button"}]
  };

  // Create a notification.
  create("testId", basicNotificationOptions)
      // Then update a few items
      .then(function() {
        return update("testId", {
          title: "Changed!",
          message: "Too late! The show ended yesterday",
          silent: true
        });
      })
      // Then update a few more items
      .then(function() {
        return update("testId", {priority: 2, buttons: [{title: "NewButton"}]});
      })
      // The test will continue in C++, checking that all the updates "took"
      .then(chrome.test.succeed, chrome.test.fail);
};


chrome.test.runTests([testPartialUpdate]);
