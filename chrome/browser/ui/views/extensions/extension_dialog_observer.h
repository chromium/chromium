// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_OBSERVER_H_

class ExtensionDialog;

// Observer to ExtensionDialog events.
class ExtensionDialogObserver {
 public:
  ExtensionDialogObserver();
  virtual ~ExtensionDialogObserver();

  // Called when the ExtensionDialog is closing. Note that it
  // is ref-counted, and thus will be released shortly after
  // making this delegate call.
  virtual void ExtensionDialogClosing(ExtensionDialog* popup) = 0;
  // Called in case the extension hosted by the extension dialog is
  // terminated, e.g. if the extension crashes.
  virtual void ExtensionTerminated(ExtensionDialog* popup) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_OBSERVER_H_
