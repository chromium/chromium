// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TOOLBAR_MODEL_H_
#define COMPONENTS_OMNIBOX_BROWSER_TOOLBAR_MODEL_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/security_state/core/security_state.h"
#include "url/gurl.h"

namespace gfx {
struct VectorIcon;
}

// This class is the model used by the toolbar, location bar and autocomplete
// edit.  It populates its states from the current navigation entry retrieved
// from the navigation controller returned by GetNavigationController().
class ToolbarModel {
 public:
  virtual ~ToolbarModel() = default;

  // Returns the formatted full URL for the toolbar. The formatting includes:
  //   - Some characters may be unescaped.
  //   - The scheme and/or trailing slash may be dropped.
  // This method specifically keeps the URL suitable for editing by not
  // applying any elisions that change the meaning of the URL.
  virtual base::string16 GetFormattedFullURL() const = 0;

  // Returns a simplified URL for display (but not editing) on the toolbar.
  // This formatting is generally a superset of GetFormattedFullURL, and may
  // include some destructive elisions that change the meaning of the URL.
  // The returned string is not suitable for editing, and is for display only.
  virtual base::string16 GetURLForDisplay() const = 0;

  // Returns the URL of the current navigation entry.
  virtual GURL GetURL() const = 0;

  // Returns the security level that the toolbar should display.  If
  // |ignore_editing| is true, the result reflects the underlying state of the
  // page without regard to any user edits that may be in progress in the
  // omnibox.
  virtual security_state::SecurityLevel GetSecurityLevel(
      bool ignore_editing) const = 0;

  // Returns the id of the icon to show to the left of the address, based on the
  // current URL.  When search term replacement is active, this returns a search
  // icon.  This doesn't cover specialized icons while the user is editing; see
  // OmniboxView::GetIcon().
  virtual const gfx::VectorIcon& GetVectorIcon() const = 0;

  // Returns text for the omnibox secure verbose chip, displayed next to the
  // security icon on certain platforms.
  virtual base::string16 GetSecureVerboseText() const = 0;

  // Returns text describing the security state for accessibility.
  virtual base::string16 GetSecureAccessibilityText() const = 0;

  // Returns the name of the EV cert holder.  This returns an empty string if
  // the security level is not EV_SECURE.
  virtual base::string16 GetEVCertName() const = 0;

  // Returns whether the URL for the current navigation entry should be
  // in the location bar.
  virtual bool ShouldDisplayURL() const = 0;

  // Returns whether the page is an offline page, sourced from a cache of
  // previously-downloaded content.
  virtual bool IsOfflinePage() const = 0;

  // Whether the text in the omnibox is currently being edited.
  void set_input_in_progress(bool input_in_progress) {
    input_in_progress_ = input_in_progress;
  }
  bool input_in_progress() const { return input_in_progress_; }

 protected:
  ToolbarModel() : input_in_progress_(false) {}

 private:
  bool input_in_progress_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarModel);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TOOLBAR_MODEL_H_
