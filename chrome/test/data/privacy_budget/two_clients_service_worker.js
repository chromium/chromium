// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener("message", async () => {
  const clients = await self.clients.matchAll({
    includeUncontrolled: true,
    type: "all",
  });

  if (clients.length == 2) {
    clients.forEach(client => client.postMessage("Done"));
  }
});
