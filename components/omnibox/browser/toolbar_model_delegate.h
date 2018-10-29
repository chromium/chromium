// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TOOLBAR_MODEL_DELEGATE_H_
#define COMPONENTS_OMNIBOX_BROWSER_TOOLBAR_MODEL_DELEGATE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "components/security_state/core/security_state.h"

class GURL;

namespace gfx {
struct VectorIcon;
}

namespace net {
class X509Certificate;
}

// Delegate which is used by ToolbarModel class.
class ToolbarModelDelegate {
 public:
  using SecurityLevel = security_state::SecurityLevel;

  // Formats |url| using AutocompleteInput::FormattedStringWithEquivalentMeaning
  // providing an appropriate AutocompleteSchemeClassifier for the embedder.
  virtual base::string16 FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const base::string16& formatted_url) const = 0;

  // Returns true and sets |url| to the current navigation entry URL if it
  // exists. Otherwise returns false and leaves |url| unmodified.
  virtual bool GetURL(GURL* url) const = 0;

  // Returns whether the URL for the current navigation entry should be
  // in the location bar.
  virtual bool ShouldDisplayURL() const;

  // Returns the underlying security level of the page without regard to any
  // user edits that may be in progress.
  virtual SecurityLevel GetSecurityLevel() const;

  // Returns the certificate for the current navigation entry.
  virtual scoped_refptr<net::X509Certificate> GetCertificate() const;

  // Returns true if the current page fails the billing interstitial check.
  virtual bool FailsBillingCheck() const;

  // Returns true if the current page fails the malware check.
  virtual bool FailsMalwareCheck() const;

  // Returns the id of the icon to show to the left of the address, or nullptr
  // if the icon should be selected by the caller. This is useful for
  // associating particular URLs with particular schemes without importing
  // knowledge of those schemes into this component.
  virtual const gfx::VectorIcon* GetVectorIconOverride() const;

  // Returns whether the page is an offline page, sourced from a cache of
  // previously-downloaded content.
  virtual bool IsOfflinePage() const;

 protected:
  virtual ~ToolbarModelDelegate() {}
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TOOLBAR_MODEL_DELEGATE_H_
