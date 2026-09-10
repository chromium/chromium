// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function goBackButtonClicked() {
  if (window.history.length > 1) {
    window.history.back();
  } else {
    window.close();
  }
}

function signinButtonClicked() {
  if (window.errorPageController) {
    window.errorPageController.portalSigninButtonClick();
  } else {
    window.location.reload();
  }
}

document.addEventListener('DOMContentLoaded', () => {
  const gobackButton = document.getElementById('goback-button');
  if (gobackButton) {
    gobackButton.addEventListener('click', goBackButtonClicked);
  }
  const signinButton = document.getElementById('signin-button');
  if (signinButton) {
    signinButton.addEventListener('click', signinButtonClicked);
  }

  const categoryElem = document.getElementById('error-category');
  if (categoryElem) {
    const category = categoryElem.textContent.trim();
    // Category 0 is kAuthentication -> show Continue (Sign In) button.
    // Category 1 (kAuthorization) and 2 (kOther) -> show Go back button.
    if (category === '0') {
      if (gobackButton) {
        gobackButton.hidden = true;
      }
      if (signinButton) {
        signinButton.hidden = false;
      }
    } else {
      if (signinButton) {
        signinButton.hidden = true;
      }
      if (gobackButton) {
        gobackButton.hidden = false;
      }
    }
  }
});
