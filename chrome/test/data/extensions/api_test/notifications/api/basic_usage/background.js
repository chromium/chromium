// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const notifications = chrome.notifications;

const red_dot = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUA" +
    "AAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO" +
    "9TXL0Y4OHwAAAABJRU5ErkJggg==";

// An image URL larger than the max size allowed for a notification image.
// This was generated using the function that follows. Should we increase
// the max size of an image, update the function to increase the width and
// height and run the function again to log the URL.
const bigImageUrl = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAA" +
    "AGQAAABkCAYAAABw4pVUAAABZUlEQVR4Xu3TQREAAAiEQK9/aWvsAxMw4O06" +
    "ysAommCuINgTFKQgmAEMp4UUBDOA4bSQgmAGMJwWUhDMAIbTQgqCGcBwWkhB" +
    "MAMYTgspCGYAw2khBcEMYDgtpCCYAQynhRQEM4DhtJCCYAYwnBZSEMwAhtNC" +
    "CoIZwHBaSEEwAxhOCykIZgDDaSEFwQxgOC2kIJgBDKeFFAQzgOG0kIJgBjCc" +
    "FlIQzACG00IKghnAcFpIQTADGE4LKQhmAMNpIQXBDGA4LaQgmAEMp4UUBDOA" +
    "4bSQgmAGMJwWUhDMAIbTQgqCGcBwWkhBMAMYTgspCGYAw2khBcEMYDgtpCCY" +
    "AQynhRQEM4DhtJCCYAYwnBZSEMwAhtNCCoIZwHBaSEEwAxhOCykIZgDDaSEF" +
    "wQxgOC2kIJgBDKeFFAQzgOG0kIJgBjCcFlIQzACG00IKghnAcFpIQTADGE4L" +
    "KQhmAMNpIQXBDGA4LQQL8oTPAGUY76lBAAAAAElFTkSuQmCC";

// Used to generate a data URL with an image that's too large. This
// will not run using a Service Worker-based extension.
function logBigImageUrl() {
  var canvas = document.createElement('canvas');
  // This is just enough to be too large for an icon. See
  // message_center::kNotificationIconSize.
  canvas.width = 100;
  canvas.height = 100;
  var ctx = canvas.getContext('2d');
  ctx.fillStyle = "rgb(200, 0, 0)";
  ctx.fillRect(10, 20, 30, 40);
  console.log(canvas.toDataURL());
}

var basicNotificationOptions = {
  type: "basic",
  title: "Basic title",
  message: "Basic message",
  iconUrl: red_dot
};

function create(id, options) {
  return new Promise(function (resolve, reject) {
    notifications.create(id, options, function (id) {
      if (chrome.runtime.lastError) {
        reject(new Error("Unable to create notification"));
        return;
      }
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

function testIdUsage() {
  var testName = "testIdUsage";
  console.log("Starting testIdUsage.");
  var succeed = succeedTest(testName);
  var fail = failTest(testName);

  var createNotification = function (idString) {
    var options = {
      type: "basic",
      iconUrl: red_dot,
      title: "Attention!",
      message: "Check out Cirque du Soleil"
    };

    return create(idString, options);
  };

  var updateNotification = function (idString) {
    var options = { title: "!", message: "!" };
    return update(idString, options);
  };

  // Should successfully create the notification
  createNotification("foo")
    // And update it.
    .then(updateNotification)
    .catch(fail)
    // Next try to update a non-existent notification.
    .then(function () { return updateNotification("foo2"); })
    // And fail if it returns true.
    .then(fail)
    // Next try to clear a non-existent notification.
    .catch(function () { return clear("foo2"); })
    .then(fail)
    // And finally clear the original notification.
    .catch(function () { return clear("foo"); })
    .catch(fail)
    .then(succeed);
};

function testIdLimit() {
  var testName = "testIdLimit";
  console.log("Starting " + testName);
  var succeed = succeedTest(testName);
  var fail = failTest(testName);

  // Notification Ids are limited to 500 characters in length.
  var id = 'a'.repeat(501);

  create(id, {
    type: 'basic',
    iconUrl: red_dot,
    title: 'My title',
    message: 'My message'
  }).then(fail, succeed);
}

function testBaseFormat() {
  var testName = "testBaseFormat";
  console.log("Starting " + testName);
  var succeed = succeedTest(testName);
  var fail = failTest(testName);

  var createNotificationWithout = function(toDelete) {
    var options = {
      type: "basic",
      iconUrl: red_dot,
      title: "Attention!",
      message: "Check out Cirque du Soleil",
      contextMessage: "Foobar.",
      priority: 1,
      eventTime: 123457896.12389,
      expandedMessage: "This is a longer expanded message.",
    };

    for (var i = 0; i < toDelete.length; i++) {
      delete options[toDelete[i]];
    }

    return create("", options);
  };

  // Construct some exclusion lists.  The |createNotificationWithout| function
  // starts with a complex notification and then deletes items in this list.
  var basicNotification= [
    "buttons",
    "items",
    "progress",
    "imageUrl"
  ];
  var bareNotification = basicNotification.concat([
    "priority",
    "eventTime",
    "expandedMessage",
  ]);
  var basicNoType = basicNotification.concat(["type"]);
  var basicNoIcon = basicNotification.concat(["iconUrl"]);
  var basicNoTitle = basicNotification.concat(["title"]);
  var basicNoMessage = basicNotification.concat(["message"]);

  // Try creating a basic notification with just some of the fields.
  createNotificationWithout(basicNotification)
    // Try creating a basic notification with all possible fields.
    .then(function () { return createNotificationWithout([]); })
    // Try creating a basic notification with the minimum in fields.
    .then(function () { return createNotificationWithout(bareNotification); })
    // After this line we are checking to make sure that there is an error
    // when notifications are created without the proper fields.
    .catch(fail)
    // Error if no type.
    .then(function () { return createNotificationWithout(basicNoType) })
    // Error if no icon.
    .catch(function () { return createNotificationWithout(basicNoIcon) })
    // Error if no title.
    .catch(function () { return createNotificationWithout(basicNoTitle) })
    // Error if no message.
    .catch(function () { return createNotificationWithout(basicNoMessage) })
    .then(fail, succeed);
};

function testListItem() {
  var testName = "testListItem";
  console.log("Starting " + testName);
  var succeed = succeedTest(testName);
  var fail = failTest(testName);

  var item = { title: "Item title.", message: "Item message." };
  var options = {
    type: "list",
    iconUrl: red_dot,
    title: "Attention!",
    message: "Check out Cirque du Soleil",
    contextMessage: "Foobar.",
    priority: 1,
    eventTime: 123457896.12389,
    items: [item, item, item, item, item],
  };
  create("id", options).then(succeed, fail);
};

function arrayEquals(a, b) {
  if (a === b) return true;
  if (a == null || b == null) return false;
  if (a.length !== b.length) return false;

  for (var i = 0; i < a.length; i++) {
    if (a[i] !== b[i]) return false;
  }
  return true;
};

function testGetAll() {
  var testName = "testGetAll";
  console.log("Starting " + testName);
  var succeed = succeedTest(testName);
  var fail = failTest(testName);
  var in_ids = ["a", "b", "c", "d"];

  // First do a get all, make sure the list is empty.
  getAll()
    .then(function (ids) {
        chrome.test.assertEq(0, ids.length);
    })
    // Then create a bunch of notifications.
    .then(function () {
      var newNotifications = in_ids.map(function (id) {
        return create(id, basicNotificationOptions);
      });
      return Promise.all(newNotifications);
    })
    // Try getAll again.
    .then(function () { return getAll(); })
    // Check that the right set of notifications is in the center.
    .then(function (ids) {
      chrome.test.assertEq(4, ids.length);
      chrome.test.assertTrue(arrayEquals(ids, in_ids));
      succeed();
    }, fail);
}

function testProgress() {
  var testName = "testProgress";
  console.log("Starting " + testName);
  var succeed = succeedTest(testName);
  var fail = failTest(testName);
  var progressOptions = {
    type: "progress",
    title: "Basic title",
    message: "Basic message",
    iconUrl: red_dot,
    progress: 30
  };

  // First, create a basic progress notification.
  create("progress", progressOptions)
    // and update it to have a different progress level.
    .then(function () { return update("progress", { progress: 60 }); })
    // If either of the above failed, the test fails.
    .catch(fail)
    // Now the following parts should all cause an error:
    // First update the progress to a low value, out-of-range
    .then(function () { return update("progress", { progress: -10 }); })
    // First update the progress to a high value, out-of-range
    .then(fail, function () { return update("progress", { progress: 101 }); })
    .then(function () { return clear("progress"); })
    // Finally try to create a notification that has a progress value but not
    // progress type.
    .then(fail, function () {
      progressOptions.type = "basic";
      return create("progress", progressOptions);
    }).then(fail, succeed);
}

function testLargeImage() {
  var testName = "testLargeImage";
  console.log("Starting " + testName);
  var succeed = succeedTest(testName);
  var fail = failTest(testName);
  var options = {
    type: "basic",
    title: "Basic title",
    message: "Basic message",
    iconUrl: bigImageUrl,
  };
  create("largeImage", options).then(succeed, fail);
}

function testOptionalParameters() {
  var testName = "testOptionalParameters";
  var succeed = succeedTest(testName);
  var fail = failTest(testName);
  function createCallback(notificationId) {
    new Promise(function() {
      chrome.test.assertNoLastError();
      chrome.test.assertEq("string", typeof notificationId);
      // Optional callback - should run without problems
      chrome.notifications.clear(notificationId);
      // Note: The point of the previous line is to show that a callback can be
      // optional. Because .clear is asynchronous, we have to be careful with
      // calling .clear again. Since .clear is processed in order, calling
      // clear() synchronously is okay.
      // If this assumption does not hold and leaks to flaky tests, file a bug
      // report and/or put the following call in a setTimeout call.

      // The notification should not exist any more, so clear() should fail.
      clear(notificationId).then(fail, succeed);
    }).then(null, fail);
  }
  // .create should succeed even when notificationId is omitted.
  chrome.notifications.create(basicNotificationOptions, createCallback);
}

chrome.test.runTests([
    testIdUsage, testIdLimit, testBaseFormat, testListItem, testGetAll,
    testProgress, testLargeImage, testOptionalParameters
]);
