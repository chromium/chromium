// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('AddPersonDialog', function() {
  let dialog = null;

  setup(function() {
    PolymerTest.clearBody();

    dialog = document.createElement('settings-users-add-user-dialog');
    dialog.usersPrivate_ = new settings.FakeUsersPrivate();

    document.body.appendChild(dialog);

    dialog.open();
  });

  teardown(function() {
    dialog.remove();
    dialog = null;
  });


  /**
   * Test that the dialog reacts to valid and invalid input correctly.
   */
  test('Add user', function() {
    const userInputBox = dialog.$$('#addUserInput');
    assertTrue(!!userInputBox);

    const addButton = dialog.$$('.action-button');
    assertTrue(!!addButton);
    assertTrue(addButton.disabled);
    assertTrue(!userInputBox.invalid);

    // Try to add a valid username without domain
    userInputBox.value = 'abcdef';
    assertTrue(!addButton.disabled);
    assertTrue(!userInputBox.invalid);

    // Try to add a valid username with domain
    userInputBox.value = 'abcdef@xyz.com';
    assertTrue(!addButton.disabled);
    assertTrue(!userInputBox.invalid);

    // Try to add an invalid username
    userInputBox.value = 'abcdef@';
    assertTrue(addButton.disabled);
    assertTrue(userInputBox.invalid);
  });

  test('Add duplicate user', function() {
    const userInputBox = dialog.$$('#addUserInput');
    const addButton = dialog.$$('.action-button');
    const duplicateUserEmail = 'duplicateUser@google.com';

    // Add user for the first time.
    userInputBox.value = duplicateUserEmail;
    addButton.click();
    assertEquals('', userInputBox.value);
    assertFalse(userInputBox.invalid);
    assertEquals('', userInputBox.errorMessage);

    // Add user for the second time. It should be registered as a duplicate and
    // will create an error message.
    userInputBox.value = duplicateUserEmail;
    addButton.click();
    assertEquals(duplicateUserEmail, userInputBox.value);
    assertTrue(userInputBox.invalid);
    assertNotEquals('', userInputBox.errorMessage);
  });

  test('Add new user', function() {
    const userInputBox = dialog.$$('#addUserInput');
    const addButton = dialog.$$('.action-button');
    const newUserEmail = 'newUser@google.com';

    userInputBox.value = newUserEmail;
    addButton.click();
    assertEquals('', userInputBox.value);
    assertFalse(userInputBox.invalid);
    assertEquals('', userInputBox.errorMessage);
  });

  test('Add two new users', function() {
    const userInputBox = dialog.$$('#addUserInput');
    const addButton = dialog.$$('.action-button');
    const firstUserEmail = 'firstUser@google.com';
    const secondUserEmail = 'secondUser@google.com';

    userInputBox.value = firstUserEmail;
    addButton.click();
    assertEquals('', userInputBox.value);
    assertFalse(userInputBox.invalid);
    assertEquals('', userInputBox.errorMessage);

    userInputBox.value = secondUserEmail;
    addButton.click();
    assertEquals('', userInputBox.value);
    assertFalse(userInputBox.invalid);
    assertEquals('', userInputBox.errorMessage);
  });
});
