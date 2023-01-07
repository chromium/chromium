// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_HTML_DIALOG_H_
#define CHROME_INSTALLER_UTIL_HTML_DIALOG_H_

#include <string>

// This is the interface for creating HTML-based Dialogs *before* Chrome has
// been installed or when there is a suspicion chrome is not working. In
// other words, the dialogs use another native html rendering engine. In the
// case of Windows it is the the Internet Explorer control.

#include "base/memory/raw_ptr.h"

namespace installer {

// Interface for implementing a native HTML dialog.
class HTMLDialog {
 public:
  enum DialogResult {
    HTML_DLG_ERROR = 0,    // Dialog could not be shown.
    HTML_DLG_ACCEPT = 1,   // The user accepted (accept, ok, yes buttons).
    HTML_DLG_DECLINE = 2,  // The user declined (cancel, no, abort buttons).
    HTML_DLG_RETRY = 3,    // The user wants to retry the action.
    HTML_DLG_IGNORE = 4,   // The user wants to ignore the error and continue.
    HTML_DLG_TIMEOUT = 5,  // The dialog has timed out and defaults apply.
    HTML_DLG_EXTRA = 6     // There is extra data as a string. See below.
  };

  // Callbacks that allow to tweak the appearance of the dialog.
  class CustomizationCallback {
   public:
    // Called before the native window is created. Use it to pass arbitrary
    // parameters in |extra| to the rendering engine.
    virtual void OnBeforeCreation(wchar_t** extra) = 0;
    // The native window has been created and is about to be visible. Use it
    // to customize the native |window| appearance.
    virtual void OnBeforeDisplay(void* window) = 0;

   protected:
    virtual ~CustomizationCallback() {}
  };

  virtual ~HTMLDialog() {}

  // Shows the HTML in a modal dialog. The buttons and other UI are also done
  // in HTML so each native implementation needs to map the user action into
  // one of the 6 possible results of DialogResult. Important, call this
  // method only from the main (or UI) thread.
  virtual DialogResult ShowModal(void* parent_window,
                                 CustomizationCallback* callback) = 0;

  // If the result of ShowModal() was EXTRA, the information is available
  // as a string using this method.
  virtual std::wstring GetExtraResult() = 0;
};

// Factory method for the native HTML Dialog. When done with the object use
// regular 'delete' operator to destroy the object. It might choose a
// different underlying implementation according to the url protocol.
HTMLDialog* CreateNativeHTMLDialog(const std::wstring& url,
                                   const std::wstring& param);

// This class leverages HTMLDialog to create a dialog that is suitable
// for a end-user-agreement modal dialog. The html shows a fairly standard
// EULA form with the accept and cancel buttons and an optional check box
// to opt-in for sending usage stats and crash reports.
class EulaHTMLDialog {
 public:
  // |file| points to an html file on disk or to a resource via res:// spec.
  // |param| is a string that will be passed to the dialog as a parameter via
  //         the window.dialogArguments property.
  EulaHTMLDialog(const std::wstring& file, const std::wstring& param);

  EulaHTMLDialog(const EulaHTMLDialog&) = delete;
  EulaHTMLDialog& operator=(const EulaHTMLDialog&) = delete;

  ~EulaHTMLDialog();

  enum Outcome {
    REJECTED,         // Declined EULA, mapped from HTML_DLG_ACCEPT (1).
    ACCEPTED,         // Accepted EULA no opt-in, from HTML_DLG_DECLINE (2).
    ACCEPTED_OPT_IN,  // Accepted EULA and opt-in, from HTML_DLG_EXTRA (6).
  };

  // Shows the dialog and blocks for user input. The return value is one of
  // the |Outcome| values and any form of failure maps to REJECTED.
  Outcome ShowModal();

 private:
  class Customizer : public HTMLDialog::CustomizationCallback {
   public:
    void OnBeforeCreation(wchar_t** extra) override;
    void OnBeforeDisplay(void* window) override;
  };

  raw_ptr<HTMLDialog> dialog_;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_HTML_DIALOG_H_
