// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


const first = document.querySelector('#number1');
const second = document.querySelector('#number2');
const additional = document.querySelector('#additional');

const result = document.querySelector('#result');
const form = document.querySelector('form');

const myWorker = new SharedWorker('worker.js');

first.onchange = () => {
  myWorker.port.postMessage([first.value, second.value]);
};

second.onchange = () => {
  myWorker.port.postMessage([first.value, second.value]);
};

myWorker.port.onmessage = (event) => {
  result.textContent = event.data[0];
  additional.value = event.data[1];
};
