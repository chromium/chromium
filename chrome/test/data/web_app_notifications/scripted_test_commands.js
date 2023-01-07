// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Requests permission to display Web Notifications.
async function requestPermission() {
  return Notification.requestPermission();
}

async function displayPersistentNotification() {
  return await postRequestAwaitResponse(
      {type: 'showNotification', useBadge: false, title: 'Notification Title'});
}

async function displayPersistentNotificationWithBadge() {
  return await postRequestAwaitResponse({
    type: 'showNotification',
    useBadge: true,
    title: 'Notification With Badge'
  });
}

async function closeAllPersistentNotifications() {
  return await postRequestAwaitResponse({type: 'closeAllNotifications'});
}
