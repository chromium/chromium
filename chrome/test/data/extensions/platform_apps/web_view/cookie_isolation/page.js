// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function initialize() {
  var messageHandler = Messaging.GetHandler();
  var cookieAgent = new Messaging.Agent('agent_cookie');
  messageHandler.addAgent(cookieAgent);
  // Handles the request to set cookies to this page.
  cookieAgent.addTask(SET_COOKIES, function(message, portFrom) {
    console.log('Message received: ' + SET_COOKIES);
    var cookieString =
        Cookie.convertCookieDataToString(message.content.cookieData);
    console.log('Setting cookie to: ' + cookieString);
    document.cookie = cookieString;
    messageHandler.sendMessage(
        new Messaging.Message(
            'agent_cookie', message.source, {type: SET_COOKIES_COMPLETE}),
        portFrom);
  });
  // Handles the request to read cookies.
  cookieAgent.addTask(GET_COOKIES, function(message, portFrom) {
    console.log('Cookie requested. We have: ' + document.cookie);
    messageHandler.sendMessage(
        new Messaging.Message('agent_cookies', message.source, {
          type: GET_COOKIES_COMPLETE,
          cookies: Cookie.convertStringToCookies(document.cookie)
        }),
        portFrom);
  });
  // Handles the request to clear all cookies for this page.
  cookieAgent.addTask(CLEAR_COOKIES, function(message, portFrom) {
    Cookie.deleteAllCookies();
    console.log('Deleted all cookies.');
    messageHandler.sendMessage(
        new Messaging.Message(
            'agent_cookie', message.source, {type: CLEAR_COOKIES_COMPLETE}),
        portFrom);
  });
}
window.onload = initialize;
