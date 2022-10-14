// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var clients_ports = [];

onconnect = (e) => {
  const port = e.ports[0];
  clients_ports.push(port);
  port.onmessage = (e) => {
    if (e.data == "close") {
      // Close the worker to flush out the metrics.
      self.close();
    } else if (clients_ports.length == 2) {
      clients_ports[0].postMessage("Done");
      clients_ports[1].postMessage("Done");
    }
  }
}
