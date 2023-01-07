// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DROPDOWN_BAR_HOST_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_DROPDOWN_BAR_HOST_DELEGATE_H_

class DropdownBarHostDelegate {
 public:
  // Claims focus for the text field and selects its contents.
  virtual void FocusAndSelectAll() = 0;

 protected:
  virtual ~DropdownBarHostDelegate() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_DROPDOWN_BAR_HOST_DELEGATE_H_
