// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function doPromise(promise) {
  console.log('promise: ', promise);
  promise.then(
    value => { console.log('promise resolved: ', JSON.stringify(value)); },
    err   => { console.log('promise rejected: ', JSON.stringify(err));   });
}

function callAdd() {
  var arg = {};
  var app_id = document.getElementById('subAppUrl').value;
  arg[app_id] = {'install_url': app_id};
  doPromise(navigator.subApps.add(arg));
}

function callRemove() {
  arg = document.getElementById('subAppUrl').value;
  doPromise(navigator.subApps.remove(arg));
}

function callList() {
  navigator.subApps.list().then(
    output => {
      document.getElementById('listOutput').value =
      JSON.stringify(output);
    },
    err => {
      document.getElementById('listOutput').value =
          "Error: " + JSON.stringify(err);
    }
  )
}