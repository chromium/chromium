// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Creates the following form:
// <form action="done.html">
//   <label for="username">Username</label>
//   <input type="text" id="username" name="username">
//   <label for="password">Password</label>
//   <input type="password" id="password" name="password">
//   <input type="submit" id="submit-button">
// </form>
function createSimplePasswordForm() {
  var form = document.createElement('form');
  form.setAttribute('action', 'done.html');

  var username_label = document.createElement('label');
  username_label.htmlFor = 'username';
  username_label.innerText = 'Username: ';
  var username = document.createElement('input');
  username.type = 'text';
  username.name = 'username';
  username.id = 'username';

  var password_label = document.createElement('label');
  password_label.innerText = 'Password: ';
  password_label.htmlFor = 'password';
  var password = document.createElement('input');
  password.type = 'password';
  password.name = 'password';
  password.id = 'password';

  var submit = document.createElement('input');
  submit.type = 'submit';
  submit.id = 'submit-button';
  submit.value = 'Submit';

  form.appendChild(username_label);
  form.appendChild(username);
  form.appendChild(password_label);
  form.appendChild(password);
  form.appendChild(submit);

  return form;
}
