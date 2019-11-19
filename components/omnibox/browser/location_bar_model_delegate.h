// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_DELEGATE_H_
#define COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_DELEGATE_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "components/security_state/core/security_state.h"

class AutocompleteClassifier;
class GURL;
class TemplateURLService;

namespace gfx {
struct VectorIcon;
}

namespace net {
class X509Certificate;
}

// Delegate which is used by LocationBarModel class.
class LocationBarModelDelegate {
 public:
  // Formats |url| using AutocompleteInput::FormattedStringWithEquivalentMeaning
  // providing an appropriate AutocompleteSchemeClassifier for the embedder.
  virtual base::string16 FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const base::string16& formatted_url) const = 0;

  // Returns true and sets |url| to the current navigation entry URL if it
  // exists. Otherwise returns false and leaves |url| unmodified.
  virtual bool GetURL(GURL* url) const = 0;

  // Returns whether we should prevent elision of the display URL and turn off
  // query in omnibox. Based on whether user has a specified extension enabled.
  virtual bool ShouldPreventElision() const;

  // Returns whether the URL for the current navigation entry should be
  // in the location bar.
  virtual bool ShouldDisplayURL() const;

  // Returns the underlying security level of the page without regard to any
  // user edits that may be in progress.
  virtual security_state::SecurityLevel GetSecurityLevel() const;

  // Returns the underlying security state of the page without regard to any
  // user edits that may be in progress. Should never return nullptr.
  virtual std::unique_ptr<security_state::VisibleSecurityState>
  GetVisibleSecurityState() const;

  // Returns the certificate for the current navigation entry.
  virtual scoped_refptr<net::X509Certificate> GetCertificate() const;

  // Returns the id of the icon to show to the left of the address, or nullptr
  // if the icon should be selected by the caller. This is useful for
  // associating particular URLs with particular schemes without importing
  // knowledge of those schemes into this component.
  virtual const gfx::VectorIcon* GetVectorIconOverride() const;

  // Returns whether the page is an offline page, sourced from a cache of
  // previously-downloaded content.
  virtual bool IsOfflinePage() const;

  // Returns true if the current page is a New Tab Page rendered by Instant.
  virtual bool IsInstantNTP() const;

  // Returns whether |url| corresponds to the new tab page.
  virtual bool IsNewTabPage(const GURL& url) const;

  // Returns whether |url| corresponds to the user's home page.
  virtual bool IsHomePage(const GURL& url) const;

  // Returns the AutocompleteClassifier instance for the current page.
  virtual AutocompleteClassifier* GetAutocompleteClassifier();

  // Returns the TemplateURLService instance for the current page.
  virtual TemplateURLService* GetTemplateURLService();

 protected:
  virtual ~LocationBarModelDelegate() {}
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_DELEGATE_H_
