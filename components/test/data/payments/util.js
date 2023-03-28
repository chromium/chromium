/*
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Prints the message on the page.
 * @param {String} msg - The message to print.
 */
function print(msg) {
  document.getElementById('result').innerHTML = msg;
}

/**
 * Prints output in developer console and on the page, as well as sends it to
 * the DOM automation controller.
 * @param {String} src - Human-readable description of where the message is
 *                       coming from.
 * @param {String} txt - The text to print.
 * @return {String} The text that was printed.
 */
function output(src, txt) {
  // Handle DOMException:
  if (txt && txt.message) {
    txt = txt.message;
  }
  txt = src + ': ' + txt;
  print(txt);
  console.warn(txt);
  return txt;
}
