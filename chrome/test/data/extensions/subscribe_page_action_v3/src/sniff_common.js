// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The possible log levels.
var logLevels = {
    "none": 0,
    "error": 1,
    "info": 2
};

// Defines the current log level. Values other than "none" are for debugging
// only and should at no point be checked in.
var currentLogLevel = logLevels.none;

function containsFeed(doc) {
  debugMsg(logLevels.info, "containsFeed called");

  // Find all the RSS link elements.
  var result = doc.evaluate(
      '//*[local-name()="rss" or local-name()="feed" or local-name()="RDF"]',
      doc, null, 0, null);

  if (!result) {
    debugMsg(logLevels.info, "exiting: document.evaluate returned no results");
    return false;  // This is probably overly defensive, but whatever.
  }

  var node = result.iterateNext();

  if (!node) {
    debugMsg(logLevels.info, "returning: iterateNext() returned no nodes");
    return false;  // No RSS tags were found.
  }

  // The feed for arab dash jokes dot net, for example, contains
  // a feed that is a child of the body tag so we continue only if the
  // node contains no parent or if the parent is the body tag.
  if (node.parentElement && node.parentElement.tagName != "BODY") {
    debugMsg(logLevels.info, "exiting: parentElement that's not BODY");
    return false;
  }

  debugMsg(logLevels.info, "Found feed");

  return true;
}

function debugMsg(loglevel, text) {
  if (loglevel <= currentLogLevel) {
    console.log("RSS Subscription extension: " + text);
  }
}
