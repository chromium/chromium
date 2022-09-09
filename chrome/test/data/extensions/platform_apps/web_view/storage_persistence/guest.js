// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Date of tomorrow
function tomorrow() {
  var date = new Date(Date.now());
  date.setDate(date.getDate() + 1);
  return date;
}

// Get a cookie, add proper path and the date of tomorrow and then return it as
// a string.
function getCookieExpiringTomorrow(cookie) {
  return JSON.stringify(cookie)
      .split('"').join('')
      .replace('{', '')
      .replace('}', '')
      .replace(':', '=') +
      ';' + ' path=/;' + ' expires=' + tomorrow() + ';';
}

// Reads cookies from document and converts them into an object where the key is
// the cookie name and the value is the cookie's value.
function getCookiesFromDocument() {
  var cookies = {};
  var cookieTerms = document.cookie.split(';');
  for (var index = 0; index < cookieTerms.length; ++index) {
    if (cookieTerms[index].indexOf('=') !== -1) {
      var parts = cookieTerms[index].split('=');
      cookies[parts[0].trim()] = parts[1].trim();
    }
  }
  return cookies;
}

// Defines proper message handlers for this window. The messages which are
// handled include commands from the app window to set and get cookies.
function initialize() {
  var messageHandler = Messaging.GetHandler();
  // Handler to set the cookies as requested.
  messageHandler.addHandler(SET_COOKIE, function(message, portFrom) {
    document.cookie = getCookieExpiringTomorrow(message.cookie);
    messageHandler.sendMessage(new Messaging.Message(SET_COOKIE_COMPLETE, {}),
        portFrom);
  });
  // Handler to send the cookies to the requester.
  messageHandler.addHandler(GET_COOKIES, function(message, portFrom) {
    messageHandler.sendMessage(
        new Messaging.Message(
            GET_COOKIES_COMPLETE, {cookies: getCookiesFromDocument()}),
        portFrom);
  });
}
window.onload = initialize;
