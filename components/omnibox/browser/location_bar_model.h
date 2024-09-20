// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_H_
#define COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_H_

#include <stddef.h>

#include <string>

#include "components/omnibox/common/omnibox_focus_state.h"
#include "components/security_state/core/security_state.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"

namespace gfx {
struct VectorIcon;
}

// This class provides information about the current navigation entry.
// Its methods always return data related to the current page, and does not
// account for the state of the omnibox, which is tracked by OmniboxEditModel.
class LocationBarModel {
 public:
  virtual ~LocationBarModel() = default;

  // Returns the formatted full URL for the toolbar. The formatting includes:
  //   - Some characters may be unescaped.
  //   - The scheme and/or trailing slash may be dropped.
  // This method specifically keeps the URL suitable for editing by not
  // applying any elisions that change the meaning of the URL.
  virtual std::u16string GetFormattedFullURL() const = 0;

  // Returns a simplified URL for display (but not editing) on the toolbar.
  // This formatting is generally a superset of GetFormattedFullURL, and may
  // include some destructive elisions that change the meaning of the URL.
  // The returned string is not suitable for editing, and is for display only.
  virtual std::u16string GetURLForDisplay() const = 0;

  // Returns the URL of the current navigation entry.
  virtual GURL GetURL() const = 0;

  // Returns the security level that the toolbar should display.
  virtual security_state::SecurityLevel GetSecurityLevel() const = 0;

  // Returns the cert status of the current navigation entry.
  virtual net::CertStatus GetCertStatus() const = 0;

  // Classify the current page being viewed as, for example, the new tab
  // page or a normal web page.  Used for logging omnibox events for
  // UMA opted-in users.  Examines the user's profile to determine if the
  // current page is the user's home page.
  virtual metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch = false) const = 0;

  // Returns the id of the icon to show to the left of the address, based on the
  // current URL.  When search term replacement is active, this returns a search
  // icon.
  virtual const gfx::VectorIcon& GetVectorIcon() const = 0;

  // Returns text for the omnibox secure verbose chip, displayed next to the
  // security icon on certain platforms.
  virtual std::u16string GetSecureDisplayText() const = 0;

  // Returns text describing the security state for accessibility.
  virtual std::u16string GetSecureAccessibilityText() const = 0;

  // Returns whether the URL for the current navigation entry should be
  // in the location bar.
  virtual bool ShouldDisplayURL() const = 0;

  // Returns whether the page is an offline page, sourced from a cache of
  // previously-downloaded content.
  virtual bool IsOfflinePage() const = 0;

  // Returns whether to prevent elision of the display URL, based on whether
  // user has a specified extension or pref enabled. If true, the only elisions
  // should be username/password and trailing slash on bare hostname.
  virtual bool ShouldPreventElision() const = 0;

  // Returns whether the omnibox should use the new security indicators for
  // secure HTTPS connections.
  virtual bool ShouldUseUpdatedConnectionSecurityIndicators() const = 0;

 protected:
  LocationBarModel() = default;

  LocationBarModel(const LocationBarModel&) = delete;
  LocationBarModel& operator=(const LocationBarModel&) = delete;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_H_
