// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Cookie = {};
// Holds the basic information to write a simple cookie.
Cookie.CookieData = function(key, value, path, expire) {
  this.key = key;
  this.value = value;
  this.path = path;
  this.expire = expire + '';
};

/**
 * @param {CookieDate} cdata - The new cookie information which includes
 *                             |expire| and |path| fields.
 * @returns {String} A string which represents this cookie and can be used
 *                   to insert
 * cookie by using |document.cookie = VALUE|.
 */
Cookie.convertCookieDataToString = function(cdata) {
  return cdata.key + '=' + cdata.value + ';' + ' path=' + cdata.path + ";" +
      ' expires=' + cdata.expire + ';';
}

/**
 * Parses a cookie string obtained from |document.cookie| and converts it into
 * an object (dictionary) of key-value's.
 * @param {String} str - The input cookie string.
 * @returns {Object} A dictionary of key values representing the cookies inside
 *                   the given string.
 */
Cookie.convertStringToCookies = function(str) {
  var cookies = {};
  var tokens = str.split(';');
  for (var i  = 0; i < tokens.length; i++) {
    var parts = tokens[i].split('=');
    if (parts.length != 2) {
      continue;
    }
    var key = parts[0].trim();
    var value = parts[1].trim();
    cookies[key] = value;
  }
  return cookies;
}

/**
 * Deletes all the cookies stored on this page.
 */
Cookie.deleteAllCookies = function() {
  var cookies = document.cookie.split(';');
  for (var i = 0; i < cookies.length; i++) {
    var key = cookies[i].split("=")[0];
    document.cookie = key + "=; expires=Thu, 01 Jan 1970 00:00:00 GMT;";
  }
}
