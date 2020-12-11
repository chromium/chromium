// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_EDIT_CONTROLLER_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_EDIT_CONTROLLER_H_

#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/search_engines/template_url.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class LocationBarModel;

class OmniboxEditController {
 public:
  virtual void OnAutocompleteAccept(const GURL& destination_url,
                                    TemplateURLRef::PostContent* post_content,
                                    WindowOpenDisposition disposition,
                                    ui::PageTransition transition,
                                    AutocompleteMatchType::Type match_type,
                                    base::TimeTicks match_selection_timestamp);

  virtual void OnInputInProgress(bool in_progress);

  // Called when anything has changed that might affect the layout or contents
  // of the views around the edit, including the text of the edit and the
  // status of any keyword- or hint-related state.
  virtual void OnChanged();

  // Called when the omnibox popup is shown or hidden.
  virtual void OnPopupVisibilityChanged();

  virtual LocationBarModel* GetLocationBarModel() = 0;
  virtual const LocationBarModel* GetLocationBarModel() const = 0;

 protected:
  OmniboxEditController();
  virtual ~OmniboxEditController();
  OmniboxEditController(const OmniboxEditController&) = delete;
  OmniboxEditController& operator=(const OmniboxEditController&) = delete;

  GURL destination_url() const { return destination_url_; }
  TemplateURLRef::PostContent* post_content() const { return post_content_; }
  WindowOpenDisposition disposition() const { return disposition_; }
  ui::PageTransition transition() const { return transition_; }
  base::TimeTicks match_selection_timestamp() const {
    return match_selection_timestamp_;
  }

 private:
  // The details necessary to open the user's desired omnibox match.
  GURL destination_url_;
  TemplateURLRef::PostContent* post_content_;
  WindowOpenDisposition disposition_;
  ui::PageTransition transition_;
  base::TimeTicks match_selection_timestamp_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_EDIT_CONTROLLER_H_
