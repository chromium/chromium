// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('notificationclick', (e) => {
  console.assert('url' in e.notification.data);
  clients.openWindow(e.notification.data.url);
  e.notification.close();
});

