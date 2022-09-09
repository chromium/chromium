// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var load_util = {
  report: function(msg) {
    // The automation controller seems to choke on Objects, so turn them into
    // strings.
    domAutomationController.send(JSON.stringify(msg));
  },

  log: function(message) {
    load_util.report({type: "Log", message: message});
  },

  shutdown: function(message, passed) {
    load_util.report({type: "Shutdown", message: message, passed: passed});
  },

  embed: function(manifest_url) {
    var embed = document.createElement("embed");
    embed.src = manifest_url;
    embed.type = "application/x-nacl";
    return embed;
  },

  // Use the DOM to determine the absolute URL.
  absoluteURL: function(url) {
    var a = document.createElement("a");
    a.href = url;
    return a.href;
  },

  crossOriginURL: function(manifest_url) {
    manifest_url = load_util.absoluteURL(manifest_url);
    // The test server is only listening on a specific random port
    // at 127.0.0.1. So, to inspect a cross-origin request from within
    // the server code, we load from "localhost" which is a different origin,
    // yet still served by the server. Otherwise, if we choose a host
    // other than localhost we would need to modify the DNS/host resolver
    // to point that host at 127.0.0.1.
    var cross_url = manifest_url.replace("127.0.0.1", "localhost");
    if (cross_url == manifest_url) {
      load_util.shutdown("Could not create a cross-origin URL.", false);
      throw "abort";
    }
    return cross_url;
  },

  crossOriginEmbed: function(manifest_url) {
    return load_util.embed(load_util.crossOriginURL(manifest_url));
  },

  expectLoadFailure: function(embed, message) {
    embed.addEventListener("load", function(event) {
      load_util.log("Load succeeded when it should have failed.");
      load_util.shutdown("1 test failed.", false);
    }, true);

    embed.addEventListener("error", function(event) {
      if (embed.lastError != "NaCl module load failed: " + message) {
        load_util.log("Unexpected load error: " + embed.lastError);
        load_util.shutdown("1 test failed.", false);
      } else {
        load_util.shutdown("1 test passed.", true);
      }
    }, true);
  }
};
