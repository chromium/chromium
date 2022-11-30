// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Draw some stuff into canvasElem. Doesn't really matter what.
function drawSomething(canvasElem, seed) {
  canvasElem.width = 100;
  canvasElem.height = 100;
  var ctx = canvasElem.getContext('2d');

  ctx.lineWidth = seed;
  ctx.strokeRect(0, 0, 100, 100);
  ctx.fillRect(30, 30, 40, 40);
}

// Creates a new Canvas element,
function doTheThing() {
  var parser = new DOMParser();

  var otherDocHtml = '<div id="a"></div>';

  // otherDocument is a different Document than window.document.
  var otherDocument = parser.parseFromString(otherDocHtml, 'text/html');

  // canvasElem starts out belonging to the otherDocument.
  var canvasElem = otherDocument.createElement('canvas');
  otherDocument = null;
  drawSomething(canvasElem, 10);

  var p = Promise.resolve(canvasElem);
  p.then((elem) => {
     var newElem = window.document.adoptNode(elem);
     return newElem;
   })
      .then((elem) => {
        // The identifiable surface from the following call should be
        // associated with window.document rather than the now disposed
        // other Document.
        return new Promise((resolve, reject) => {
          drawSomething(elem, 20);
          elem.toBlob(resolve);
        });
      })
      .then(() => {
        sendValueToTest('Done');
      });
}

window.addEventListener('load', () => {
  doTheThing();
})
