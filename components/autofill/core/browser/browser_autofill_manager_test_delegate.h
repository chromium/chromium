// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_TEST_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_TEST_DELEGATE_H_

namespace autofill {

class BrowserAutofillManagerTestDelegate {
 public:
  virtual ~BrowserAutofillManagerTestDelegate() {}

  // Called when a form is previewed with Autofill suggestions.
  virtual void DidPreviewFormData() = 0;

  // Called when a form is filled with Autofill suggestions.
  virtual void DidFillFormData() = 0;

  // Called when a popup with Autofill suggestions is shown.
  virtual void DidShowSuggestions() = 0;

  // Called when a text field change is detected.
  virtual void OnTextFieldChanged() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_TEST_DELEGATE_H_
