// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_IMPL_H_
#define COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_IMPL_H_

#include <stddef.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/url_formatter/url_formatter.h"
#include "url/gurl.h"

class LocationBarModelDelegate;

// This class is the model used by the toolbar, location bar and autocomplete
// edit.  It populates its states from the current navigation entry retrieved
// from the navigation controller returned by GetNavigationController().
class LocationBarModelImpl : public LocationBarModel {
 public:
  LocationBarModelImpl() = delete;

  LocationBarModelImpl(LocationBarModelDelegate* delegate,
                       size_t max_url_display_chars);

  LocationBarModelImpl(const LocationBarModelImpl&) = delete;
  LocationBarModelImpl& operator=(const LocationBarModelImpl&) = delete;

  ~LocationBarModelImpl() override;

  // LocationBarModel:
  std::u16string GetFormattedFullURL() const override;
  std::u16string GetURLForDisplay() const override;
  GURL GetURL() const override;
  security_state::SecurityLevel GetSecurityLevel() const override;
  net::CertStatus GetCertStatus() const override;
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch = false) const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetSecureDisplayText() const override;
  std::u16string GetSecureAccessibilityText() const override;
  bool ShouldDisplayURL() const override;
  bool IsOfflinePage() const override;
  bool ShouldPreventElision() const override;
  bool ShouldUseUpdatedConnectionSecurityIndicators() const override;

 private:
  std::u16string GetFormattedURL(
      url_formatter::FormatUrlTypes format_types) const;

  raw_ptr<LocationBarModelDelegate> delegate_;
  const size_t max_url_display_chars_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_IMPL_H_
