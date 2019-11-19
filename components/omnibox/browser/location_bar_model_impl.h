// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_IMPL_H_
#define COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_IMPL_H_

#include <stddef.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/url_formatter/url_formatter.h"
#include "url/gurl.h"

class LocationBarModelDelegate;

// This class is the model used by the toolbar, location bar and autocomplete
// edit.  It populates its states from the current navigation entry retrieved
// from the navigation controller returned by GetNavigationController().
class LocationBarModelImpl : public LocationBarModel {
 public:
  LocationBarModelImpl(LocationBarModelDelegate* delegate,
                       size_t max_url_display_chars);
  ~LocationBarModelImpl() override;

  // LocationBarModel:
  base::string16 GetFormattedFullURL() const override;
  base::string16 GetURLForDisplay() const override;
  GURL GetURL() const override;
  security_state::SecurityLevel GetSecurityLevel() const override;
  bool GetDisplaySearchTerms(base::string16* search_terms) override;
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      OmniboxFocusSource focus_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  base::string16 GetSecureDisplayText() const override;
  base::string16 GetSecureAccessibilityText() const override;
  bool ShouldDisplayURL() const override;
  bool IsOfflinePage() const override;

 private:
  struct SecureChipText {
    base::string16 display_text_;
    base::string16 accessibility_label_;
    SecureChipText(base::string16 display_text,
                   base::string16 accessibility_label)
        : display_text_(display_text),
          accessibility_label_(accessibility_label) {}
    SecureChipText(base::string16 display_text)
        : display_text_(display_text), accessibility_label_(display_text) {}
  };

  // Get the security chip labels for the current security state. Always returns
  // the text corresponding to the currently displayed page, irrespective of any
  // user input in progress or displayed suggestions.
  SecureChipText GetSecureChipText() const;

  base::string16 GetFormattedURL(
      url_formatter::FormatUrlTypes format_types) const;

  // Extracts search terms from |url|. Returns an empty string if |url| is not
  // from the default search provider, if there are no search terms in |url|,
  // or if the extracted search terms look too much like a URL.
  base::string16 ExtractSearchTermsInternal(const GURL& url);

  LocationBarModelDelegate* delegate_;
  const size_t max_url_display_chars_;

  // Because extracting search terms from a URL string is relatively expensive,
  // and we want to support cheap calls to GetDisplaySearchTerms, cache the
  // result of the last-parsed URL string.
  base::string16 cached_search_terms_;
  GURL cached_url_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(LocationBarModelImpl);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_IMPL_H_
